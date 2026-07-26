// tests/test_run_orchestrator.cc — Integration tests for RunOrchestrator.
//
// Per Section 5.3 of req.md:
//  - SINGLE_SHOT: sequence == 0, timestamp set, exactly one result batch
//  - LONG_TERM: monotonic sequences, timestamps monotonic-ish
//  - TIMEOUT synthesis: HAL throws → status == TIMEOUT
//  - ABORTED synthesis: stop requested → final result status == ABORTED
//
// Uses an in-memory Spool (:memory:) and a no-op StreamClient stub.

#include <gtest/gtest.h>

#include "run/RunOrchestrator.h"
#include "hal_bridge/ResultSerializer.h"
#include "client_service.pb.h"
#include "modules/cpu.pb.h"  // hal::modules::cpu::Result — the §5 module-specific payload type

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <vector>
#include <atomic>

namespace {

using namespace arches::client;
using namespace arches::stream;

// ─── Minimal Spool (in-memory) ────────────────────────────────────────────────

std::unique_ptr<Spool> make_mem_spool() {
    static std::atomic<int> counter{0};
    // Use a unique temp path for each test.
    auto path = std::filesystem::temp_directory_path() /
                ("test_spool_" + std::to_string(counter++) + ".db");
    return std::make_unique<Spool>(path.string(), 64 * 1024 * 1024, 16 * 1024 * 1024);
}

// ─── No-op StreamClient stub ─────────────────────────────────────────────────
// We use the real StreamClient but don't connect; send() returns false.
// Batches are still spooled so we can read them back.

StreamClient make_stub_stream() {
    StreamClientConfig cfg;
    cfg.server_address = "127.0.0.1";
    cfg.server_port    = 19999;  // Nothing listening here
    return StreamClient{cfg, [](const ServerMessage&) {}};
}

// ─── HAL stubs ────────────────────────────────────────────────────────────────

// Returns a single PASS result per call.
static std::vector<hal::TestResult> hal_stub_pass(
    const std::string& module_id, int /*mode*/, const std::vector<uint8_t>&)
{
    hal::TestResult r;
    r.module_id     = module_id;
    r.test_id       = "stub_test";
    r.name          = "Stub Test";
    r.output.status = hal::Status::PASS;
    r.output.severity = hal::Severity::INFO;
    r.output.metrics["value"] = int64_t{1};
    return {r};
}

// Throws to simulate a HAL exception / timeout.
static std::vector<hal::TestResult> hal_stub_throw(
    const std::string&, int, const std::vector<uint8_t>&)
{
    throw std::runtime_error("simulated HAL timeout");
}

// Returns empty (EVENT_BASED: no event fired).
static std::vector<hal::TestResult> hal_stub_empty(
    const std::string&, int, const std::vector<uint8_t>&)
{
    return {};
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

ConfigureRun make_configure(const std::string& run_id,
                             const std::string& module_id = "cpu",
                             int mode = 1 /* SINGLE_SHOT */) {
    ConfigureRun cfg;
    cfg.set_run_id(run_id);
    cfg.set_module_id(module_id);
    cfg.set_mode(static_cast<hal::common::ExecutionMode>(mode));
    return cfg;
}

// ─── SINGLE_SHOT tests ────────────────────────────────────────────────────────

TEST(RunOrchestrator, SingleShotSequenceIsZero) {
    auto spool = make_mem_spool();
    auto stream = make_stub_stream();
    RunOrchestrator orch(*spool, stream, hal_stub_pass);

    orch.on_configure(make_configure("run1", "cpu", 1));
    orch.on_start("run1");
    // Give the driver thread time to complete.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    orch.stop_all();

    auto batches = spool->fetch_unacked(10);
    ASSERT_FALSE(batches.empty());

    // Deserialize first batch and verify sequence == 0.
    TestResultBatch batch;
    ASSERT_TRUE(batch.ParseFromArray(batches[0].payload.data(),
                                     static_cast<int>(batches[0].payload.size())));
    ASSERT_GE(batch.samples_size(), 1);
    // §5 contract: payload is the module-specific Result (hal.modules.cpu.Result)
    // wrapping the hal.common.TestResult list. Unpack it, then read results(0).
    hal::modules::cpu::Result mod_result;
    ASSERT_TRUE(batch.samples(0).payload().UnpackTo(&mod_result));
    ASSERT_GE(mod_result.results_size(), 1);
    EXPECT_EQ(mod_result.results(0).sequence(), 0u);
}

TEST(RunOrchestrator, SingleShotTimestampSet) {
    auto spool = make_mem_spool();
    auto stream = make_stub_stream();
    RunOrchestrator orch(*spool, stream, hal_stub_pass);

    auto before = std::chrono::system_clock::now();
    orch.on_configure(make_configure("run2", "cpu", 1));
    orch.on_start("run2");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    orch.stop_all();
    auto after = std::chrono::system_clock::now();

    auto batches = spool->fetch_unacked(10);
    ASSERT_FALSE(batches.empty());

    TestResultBatch batch;
    ASSERT_TRUE(batch.ParseFromArray(batches[0].payload.data(),
                                     static_cast<int>(batches[0].payload.size())));
    ASSERT_GE(batch.samples_size(), 1);

    // §5 contract: unpack the module-specific Result wrapper.
    hal::modules::cpu::Result mod_result;
    ASSERT_TRUE(batch.samples(0).payload().UnpackTo(&mod_result));
    ASSERT_GE(mod_result.results_size(), 1);

    auto ts = arches::client::hal_bridge::from_proto(mod_result.results(0).timestamp());
    EXPECT_GE(ts, before);
    EXPECT_LE(ts, after);
}

TEST(RunOrchestrator, SingleShotTerminalBatch) {
    auto spool = make_mem_spool();
    auto stream = make_stub_stream();
    RunOrchestrator orch(*spool, stream, hal_stub_pass);

    orch.on_configure(make_configure("run3", "cpu", 1));
    orch.on_start("run3");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    orch.stop_all();

    auto batches = spool->fetch_unacked(10);
    ASSERT_FALSE(batches.empty());
    TestResultBatch batch;
    ASSERT_TRUE(batch.ParseFromArray(batches[0].payload.data(),
                                     static_cast<int>(batches[0].payload.size())));
    EXPECT_TRUE(batch.terminal());
}

// ─── HAL exception → ERROR synthesis ─────────────────────────────────────────

TEST(RunOrchestrator, HalExceptionProducesErrorResult) {
    auto spool = make_mem_spool();
    auto stream = make_stub_stream();
    RunOrchestrator orch(*spool, stream, hal_stub_throw);

    orch.on_configure(make_configure("run_err", "cpu", 1));
    orch.on_start("run_err");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    orch.stop_all();

    auto batches = spool->fetch_unacked(10);
    ASSERT_FALSE(batches.empty());

    TestResultBatch batch;
    ASSERT_TRUE(batch.ParseFromArray(batches[0].payload.data(),
                                     static_cast<int>(batches[0].payload.size())));
    ASSERT_GE(batch.samples_size(), 1);
    EXPECT_EQ(batch.samples(0).status(), hal::common::TEST_STATUS_ERROR);
}

// ─── Monotonic sequences for LONG_TERM ────────────────────────────────────────

TEST(RunOrchestrator, LongTermMonotonicSequence) {
    auto spool = make_mem_spool();
    auto stream = make_stub_stream();

    // Use a counter to verify monotonicity.
    std::vector<uint64_t> sequences;
    std::mutex seq_mtx;

    auto hal_counting = [&](const std::string& module_id, int mode,
                             const std::vector<uint8_t>& cfg) -> std::vector<hal::TestResult> {
        return hal_stub_pass(module_id, mode, cfg);
    };

    RunOrchestrator orch(*spool, stream, hal_counting);
    orch.on_configure(make_configure("run_lt", "cpu", 2 /* LONG_TERM */));
    orch.on_start("run_lt");

    // Let it run for ~3 ticks (1 Hz), then stop.
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    orch.on_stop("run_lt");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto batches = spool->fetch_unacked(20);
    ASSERT_GE(batches.size(), 2u);  // At least 2 non-terminal batches

    uint64_t prev_seq = 0;
    bool first = true;
    for (const auto& row : batches) {
        TestResultBatch batch;
        if (!batch.ParseFromArray(row.payload.data(),
                                   static_cast<int>(row.payload.size()))) continue;
        for (int i = 0; i < batch.samples_size(); ++i) {
            // §5 contract: unpack the module-specific Result wrapper, then
            // walk its results list for the sequence check.
            hal::modules::cpu::Result mr;
            if (!batch.samples(i).payload().UnpackTo(&mr)) continue;
            for (int j = 0; j < mr.results_size(); ++j) {
                const auto& pr = mr.results(j);
                if (first) {
                    first = false;
                    EXPECT_EQ(pr.sequence(), 0u);
                } else {
                    EXPECT_GT(pr.sequence(), prev_seq);
                }
                prev_seq = pr.sequence();
            }
        }
    }
}

// ─── ABORTED synthesis on stop ────────────────────────────────────────────────

TEST(RunOrchestrator, StopDuringLongTermProducesAborted) {
    auto spool = make_mem_spool();
    auto stream = make_stub_stream();
    RunOrchestrator orch(*spool, stream, hal_stub_pass);

    orch.on_configure(make_configure("run_ab", "cpu", 2 /* LONG_TERM */));
    orch.on_start("run_ab");
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    orch.on_stop("run_ab");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto batches = spool->fetch_unacked(20);
    ASSERT_FALSE(batches.empty());

    // Find the terminal batch.
    bool found_aborted = false;
    for (const auto& row : batches) {
        TestResultBatch batch;
        if (!batch.ParseFromArray(row.payload.data(),
                                   static_cast<int>(row.payload.size()))) continue;
        if (batch.terminal()) {
            for (int i = 0; i < batch.samples_size(); ++i) {
                if (batch.samples(i).status() == hal::common::TEST_STATUS_ABORTED) {
                    found_aborted = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_aborted);
}

// ─── RunState transitions (Task 12) ─────────────────────────────────────────

// Helper: find the terminal batch for a run_id among the spooled rows.
static bool find_terminal_state(const std::vector<BatchRow>& batches,
                                 const std::string& run_id,
                                 RunState& out_state) {
    for (const auto& row : batches) {
        if (row.run_id != run_id) continue;
        TestResultBatch batch;
        if (!batch.ParseFromArray(row.payload.data(),
                                   static_cast<int>(row.payload.size()))) continue;
        if (batch.terminal()) { out_state = batch.state(); return true; }
    }
    return false;
}

TEST(RunOrchestrator, SingleShotHalErrorSetsFailedState) {
    auto spool = make_mem_spool();
    auto stream = make_stub_stream();
    RunOrchestrator orch(*spool, stream, hal_stub_throw);

    orch.on_configure(make_configure("run_fail", "cpu", 1 /* SINGLE_SHOT */));
    orch.on_start("run_fail");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    orch.stop_all();

    auto batches = spool->fetch_unacked(10);
    ASSERT_FALSE(batches.empty());

    RunState state = RunState::RUN_STATE_UNSPECIFIED;
    ASSERT_TRUE(find_terminal_state(batches, "run_fail", state));
    EXPECT_EQ(state, RunState::FAILED);
}

TEST(RunOrchestrator, LongTermStopSetsCancelledState) {
    auto spool = make_mem_spool();
    auto stream = make_stub_stream();
    RunOrchestrator orch(*spool, stream, hal_stub_pass);

    orch.on_configure(make_configure("run_cx", "cpu", 2 /* LONG_TERM */));
    orch.on_start("run_cx");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    orch.on_stop("run_cx");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto batches = spool->fetch_unacked(20);
    ASSERT_FALSE(batches.empty());

    RunState state = RunState::RUN_STATE_UNSPECIFIED;
    ASSERT_TRUE(find_terminal_state(batches, "run_cx", state));
    EXPECT_EQ(state, RunState::CANCELLED);
}

// ─── RestartRun (Task 11) ───────────────────────────────────────────────────

TEST(RunOrchestrator, RestartStopsOldRunAndStartsNewRun) {
    auto spool = make_mem_spool();
    auto stream = make_stub_stream();
    RunOrchestrator orch(*spool, stream, hal_stub_pass);

    // Configure both runs up front so on_restart can start the new one.
    orch.on_configure(make_configure("old_run", "cpu", 1));
    orch.on_configure(make_configure("new_run", "cpu", 1));
    orch.on_start("old_run");
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Restart: old_run must stop, new_run must start and emit a batch.
    orch.on_restart("old_run", "new_run");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    orch.stop_all();

    auto batches = spool->fetch_unacked(20);
    ASSERT_FALSE(batches.empty());

    // new_run must have produced at least one batch.
    bool new_run_ran = false;
    for (const auto& row : batches) {
        if (row.run_id == "new_run") { new_run_ran = true; break; }
    }
    EXPECT_TRUE(new_run_ran);
}

} // namespace
