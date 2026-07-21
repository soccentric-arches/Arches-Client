// tests/test_app_wiring.cc
// Integration test for the app-level adapter wiring policy:
//   build_adapter_registry() (in app.cpp) registers a real adapter for every
//   HAL-backed module and a generic StubAdapter for every other module in
//   modules.yaml. This test reproduces that policy against a small fixture and
//   asserts that proto-only modules get an honest SKIPPED result while
//   HAL-backed modules dispatch to their real adapter, with no module left
//   without an adapter (no silent drop).
//
// build_adapter_registry() is a static function in app.cpp and is not exposed,
// so this test re-implements the identical policy against the public
// AdapterRegistry / ModuleRegistry / StubAdapter APIs. If app.cpp's policy
// diverges from this, the test must be updated in lockstep.

#include "registry/AdapterRegistry.h"
#include "registry/AdapterBase.h"
#include "registry/adapters/stub_adapter.h"
#include "registry/ModuleRegistry.h"
#include "lib_base.h"

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace arches::client {
namespace {

// Minimal real adapter standing in for a HAL-backed module.
class FakeRealAdapter final : public AdapterBase {
public:
    explicit FakeRealAdapter(std::string id) : id_(std::move(id)) {}
    std::string module_id() const override { return id_; }
    std::vector<hal::TestResult> run(int /*mode*/,
                                      const std::vector<uint8_t>& /*cfg*/) override {
        hal::TestResult r;
        r.module_id = id_;
        r.test_id   = "smoke";
        r.name      = "smoke";
        r.output.status = hal::Status::PASS;
        return {std::move(r)};
    }
private:
    std::string id_;
};

// Reproduce app.cpp::build_adapter_registry: real adapters for HAL-backed
// module IDs, StubAdapter catch-all for the rest.
static std::unique_ptr<AdapterRegistry> build_registry(const std::vector<ModuleEntry>& all) {
    auto reg = std::make_unique<AdapterRegistry>();
    // HAL-backed modules (present in this fixture: "cpu").
    reg->register_adapter(std::make_unique<FakeRealAdapter>("cpu"));
    // Catch-all honest stub for every module without a real adapter.
    for (const auto& m : all) {
        if (!reg->has_adapter(m.id)) {
            reg->register_adapter(std::make_unique<StubAdapter>(m.id));
        }
    }
    return reg;
}

class AppWiringTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Fixture modules.yaml: one HAL-backed module (cpu) + two proto-only
        // modules (accelerometer, i2c) that have no HAL C++ lib yet.
        const char* yaml =
            "modules:\n"
            "  - id: cpu\n"
            "    name: CPU\n"
            "    version: 1\n"
            "    status: active\n"
            "    category: compute\n"
            "    proto_package: hal.modules.cpu\n"
            "    config_type: hal.modules.cpu.Config\n"
            "    result_type: hal.modules.cpu.Result\n"
            "    source_path: src/cpu_lib/cpu_lib.h\n"
            "  - id: accelerometer\n"
            "    name: Accelerometer\n"
            "    version: 1\n"
            "    status: active\n"
            "    category: sensors\n"
            "    proto_package: hal.modules.accelerometer\n"
            "    config_type: hal.modules.accelerometer.Config\n"
            "    result_type: hal.modules.accelerometer.Result\n"
            "    source_path: src/accelerometer/accelerometer.h\n"
            "  - id: i2c\n"
            "    name: I2C\n"
            "    version: 1\n"
            "    status: active\n"
            "    category: buses\n"
            "    proto_package: hal.modules.i2c\n"
            "    config_type: hal.modules.i2c.Config\n"
            "    result_type: hal.modules.i2c.Result\n"
            "    source_path: src/i2c/i2c.h\n";
        // Create a uniquely-named temp file safely (tmpnam is unsafe).
        char tmpl[] = "/tmp/arches_appwiring_XXXXXX";
        int fd = mkstemp(tmpl);
        ASSERT_GE(fd, 0) << "mkstemp failed";
        ::close(fd);
        path_ = std::string(tmpl) + ".yaml";
        std::rename(tmpl, path_.c_str());
        std::ofstream f(path_);
        f << yaml;
        f.close();
    }
    void TearDown() override { std::remove(path_.c_str()); }
    std::string path_;
};

TEST_F(AppWiringTest, loads_all_fixture_modules) {
    // Arrange
    ModuleRegistry mods;

    // Act
    int n = mods.load(path_);

    // Assert
    EXPECT_EQ(n, 3);
    EXPECT_TRUE(mods.known("cpu"));
    EXPECT_TRUE(mods.known("accelerometer"));
    EXPECT_TRUE(mods.known("i2c"));
}

TEST_F(AppWiringTest, every_module_has_an_adapter_after_wiring) {
    // Arrange
    ModuleRegistry mods;
    mods.load(path_);
    auto reg = build_registry(mods.all());

    // Act / Assert — no module is left without an adapter (no silent drop).
    for (const auto& m : mods.all()) {
        EXPECT_TRUE(reg->has_adapter(m.id)) << "missing adapter for " << m.id;
    }
}

TEST_F(AppWiringTest, hal_backed_module_dispatches_to_real_adapter) {
    // Arrange
    ModuleRegistry mods;
    mods.load(path_);
    auto reg = build_registry(mods.all());

    // Act
    auto results = reg->invoke("cpu", /*mode=*/1, /*cfg=*/{});

    // Assert
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].module_id, "cpu");
    EXPECT_EQ(results[0].output.status, hal::Status::PASS);
}

TEST_F(AppWiringTest, proto_only_module_returns_honest_skipped) {
    // Arrange
    ModuleRegistry mods;
    mods.load(path_);
    auto reg = build_registry(mods.all());

    // Act
    auto accel = reg->invoke("accelerometer", /*mode=*/1, /*cfg=*/{});
    auto i2c   = reg->invoke("i2c", /*mode=*/1, /*cfg=*/{});

    // Assert — SKIPPED, not a fake pass, naming the unimplemented module.
    ASSERT_EQ(accel.size(), 1u);
    EXPECT_EQ(accel[0].module_id, "accelerometer");
    EXPECT_EQ(accel[0].output.status, hal::Status::SKIPPED);
    ASSERT_EQ(i2c.size(), 1u);
    EXPECT_EQ(i2c[0].module_id, "i2c");
    EXPECT_EQ(i2c[0].output.status, hal::Status::SKIPPED);
}

TEST_F(AppWiringTest, unknown_module_id_returns_error_not_skipped) {
    // Arrange
    ModuleRegistry mods;
    mods.load(path_);
    auto reg = build_registry(mods.all());

    // Act — a module id that is not in modules.yaml and has no adapter.
    auto results = reg->invoke("does_not_exist", /*mode=*/1, /*cfg=*/{});

    // Assert — ERROR (not SKIPPED): SKIPPED is reserved for known-but-unimplemented.
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].output.status, hal::Status::ERROR);
}

} // namespace
} // namespace arches::client