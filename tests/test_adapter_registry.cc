// tests/test_adapter_registry.cc
// Unit tests for AdapterRegistry dispatch and the honest-stub fallback.
//
// Covers the architectural rules from the HAL refactor:
//   - AdapterRegistry::invoke() is the only dispatch point.
//   - Missing adapter  -> single ERROR result (no silent drop, no crash).
//   - Throwing adapter  -> caught, logged, single ERROR result.
//   - StubAdapter       -> SKIPPED with an explanatory message (never a fake pass).

#include "registry/AdapterRegistry.h"
#include "registry/AdapterBase.h"
#include "registry/adapters/stub_adapter.h"
#include "lib_base.h"

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace arches::client {
namespace {

// A deterministic fake adapter that returns one PASS result carrying a
// recognizable metric, so tests can confirm dispatch end-to-end.
class FakeAdapter final : public AdapterBase {
public:
    explicit FakeAdapter(std::string id) : id_(std::move(id)) {}
    std::string module_id() const override { return id_; }
    std::vector<hal::TestResult> run(int /*mode*/,
                                      const std::vector<uint8_t>& /*cfg*/) override {
        hal::TestResult r;
        r.module_id = id_;
        r.test_id   = "fake_test";
        r.name      = "fake_test";
        r.output.status   = hal::Status::PASS;
        r.output.severity = hal::Severity::INFO;
        r.output.metrics["answer"] = static_cast<int64_t>(42);
        return {std::move(r)};
    }
private:
    std::string id_;
};

// An adapter that throws from run() to exercise the registry's catch path.
class ThrowingAdapter final : public AdapterBase {
public:
    explicit ThrowingAdapter(std::string id) : id_(std::move(id)) {}
    std::string module_id() const override { return id_; }
    std::vector<hal::TestResult> run(int /*mode*/,
                                      const std::vector<uint8_t>& /*cfg*/) override {
        throw std::runtime_error("boom from " + id_);
    }
private:
    std::string id_;
};

TEST(AdapterRegistryTest, registered_ids_lists_registered_adapter) {
    // Arrange
    AdapterRegistry reg;
    reg.register_adapter(std::make_unique<FakeAdapter>("cpu"));

    // Act
    auto ids = reg.registered_ids();

    // Assert
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], "cpu");
}

TEST(AdapterRegistryTest, has_adapter_reflects_registration) {
    // Arrange
    AdapterRegistry reg;

    // Act / Assert
    EXPECT_FALSE(reg.has_adapter("cpu"));
    reg.register_adapter(std::make_unique<FakeAdapter>("cpu"));
    EXPECT_TRUE(reg.has_adapter("cpu"));
    EXPECT_FALSE(reg.has_adapter("nonexistent"));
}

TEST(AdapterRegistryTest, invoke_dispatches_to_registered_adapter) {
    // Arrange
    AdapterRegistry reg;
    reg.register_adapter(std::make_unique<FakeAdapter>("cpu"));

    // Act
    auto results = reg.invoke("cpu", /*mode=*/1, /*cfg=*/{});

    // Assert
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].module_id, "cpu");
    EXPECT_EQ(results[0].output.status, hal::Status::PASS);
    ASSERT_TRUE(results[0].output.metrics.count("answer"));
    EXPECT_EQ(std::get<int64_t>(results[0].output.metrics["answer"]), 42);
}

TEST(AdapterRegistryTest, invoke_missing_adapter_returns_single_error_result) {
    // Arrange
    AdapterRegistry reg;  // empty — no adapter for "gpu"

    // Act
    auto results = reg.invoke("gpu", /*mode=*/1, /*cfg=*/{});

    // Assert — never empty, never crashes; one honest ERROR.
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].module_id, "gpu");
    EXPECT_EQ(results[0].output.status, hal::Status::ERROR);
    EXPECT_NE(results[0].output.error_message.find("no adapter"), std::string::npos);
}

TEST(AdapterRegistryTest, invoke_catches_adapter_exception_and_returns_error) {
    // Arrange
    AdapterRegistry reg;
    reg.register_adapter(std::make_unique<ThrowingAdapter>("thermal"));

    // Act
    auto results = reg.invoke("thermal", /*mode=*/1, /*cfg=*/{});

    // Assert
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].module_id, "thermal");
    EXPECT_EQ(results[0].output.status, hal::Status::ERROR);
    EXPECT_NE(results[0].output.error_message.find("adapter exception"), std::string::npos);
}

TEST(AdapterRegistryTest, stub_adapter_returns_skipped_with_explanation) {
    // Arrange
    StubAdapter stub("accelerometer");

    // Act
    auto results = stub.run(/*mode=*/1, /*cfg=*/{});

    // Assert — SKIPPED, not a fake pass, with a message naming the module.
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].module_id, "accelerometer");
    EXPECT_EQ(results[0].output.status, hal::Status::SKIPPED);
    EXPECT_NE(results[0].output.error_message.find("not yet implemented"), std::string::npos);
    EXPECT_NE(results[0].output.error_message.find("accelerometer"), std::string::npos);
}

TEST(AdapterRegistryTest, register_adapter_replaces_existing_for_same_id) {
    // Arrange
    AdapterRegistry reg;
    reg.register_adapter(std::make_unique<FakeAdapter>("cpu"));
    reg.register_adapter(std::make_unique<StubAdapter>("cpu"));

    // Act
    auto results = reg.invoke("cpu", /*mode=*/1, /*cfg=*/{});

    // Assert — the later registration wins (stub → SKIPPED, not PASS).
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].output.status, hal::Status::SKIPPED);
}

} // namespace
} // namespace arches::client