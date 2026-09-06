/**
 * @file src/instrumentation/Instrumentation.cc
 * @brief Optional Arches-Instrumentation reporting (see the header).
 *
 * The whole file has two shapes: a real gRPC client when the instrumentation
 * package was found at configure time, and a no-op when it was not. The
 * no-op path is not a stub with a TODO -- it is the supported configuration
 * for an image built with ARCHES_INSTRUMENTATION = "0".
 */

#include "instrumentation/Instrumentation.h"

#include <spdlog/spdlog.h>

#ifdef ARCHES_WITH_INSTRUMENTATION
#include <chrono>

#include <grpcpp/grpcpp.h>

#include "istate.grpc.pb.h"
#endif

namespace arches::client::instrumentation {

namespace {
/// Mode 0660, owned instr-istate:instr-apps. The process must be in the
/// instr-apps group; arches-client.service declares that (Task.md E-01).
constexpr const char* kSocket = "/run/instrumentation/application.sock";
} // namespace

#ifdef ARCHES_WITH_INSTRUMENTATION

struct Reporter::Impl {
    std::unique_ptr<instr::istate::ApplicationApi::Stub> stub;
};

Reporter::Reporter() : impl_(std::make_unique<Impl>()) {
    auto channel =
        grpc::CreateChannel(std::string("unix:") + kSocket, grpc::InsecureChannelCredentials());
    // Bounded: never block application startup on instrumentation.
    const auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(2);
    if (!channel->WaitForConnected(deadline)) {
        spdlog::info("instrumentation: not available at {} — continuing without it", kSocket);
        return;
    }
    impl_->stub = instr::istate::ApplicationApi::NewStub(channel);
    spdlog::info("instrumentation: connected at {}", kSocket);
}

Reporter::~Reporter() = default;

bool Reporter::connected() const { return impl_ && impl_->stub != nullptr; }

void Reporter::register_metrics() {
    if (!connected())
        return;
    // Unit and type are carried through to HQ and cannot be reconstructed
    // later, so every metric declares both.
    const struct {
        const char* name;
        const char* unit;
        const char* type;
    } kMetrics[] = {
        {"client_runs_started", "runs", "counter"},
        {"client_runs_completed", "runs", "counter"},
        {"client_runs_aborted", "runs", "counter"},
        {"client_spool_depth", "results", "gauge"},
        {"client_stream_reconnects", "events", "counter"},
    };
    for (const auto& m : kMetrics) {
        instr::istate::RegisterMetricRequest req;
        req.set_name(m.name);
        req.set_unit(m.unit);
        req.set_type(m.type);
        req.set_app(kOwner);
        grpc::ClientContext ctx;
        instr::common::Ack ack;
        const grpc::Status s = impl_->stub->RegisterMetric(&ctx, req, &ack);
        if (!s.ok())
            spdlog::warn("instrumentation: RegisterMetric({}) failed: {}", m.name,
                         s.error_message());
    }
}

void Reporter::raise(AlarmId id, const std::string& detail) {
    if (!connected())
        return;
    instr::istate::RaiseAlarmRequest req;
    req.set_alarm_id(static_cast<uint32_t>(id));
    req.set_owner(kOwner); // required (R5)
    req.set_additional_text(detail);
    grpc::ClientContext ctx;
    instr::istate::AlarmAck ack;
    const grpc::Status s = impl_->stub->RaiseAlarm(&ctx, req, &ack);
    if (!s.ok()) {
        spdlog::warn("instrumentation: RaiseAlarm(0x{:08x}) failed: {}", static_cast<uint32_t>(id),
                     s.error_message());
        return;
    }
    if (ack.needs_snapshot()) {
        // A CRITICAL alarm asked for bounded diagnostics. The client's spool
        // and log already capture the run, so record the request and let the
        // existing diagnostics carry it rather than duplicating capture here.
        spdlog::warn("instrumentation: alarm 0x{:08x} requested a diagnostic snapshot",
                     static_cast<uint32_t>(id));
    }
}

void Reporter::clear(AlarmId id) {
    if (!connected())
        return;
    instr::istate::ClearAlarmRequest req;
    req.set_alarm_id(static_cast<uint32_t>(id));
    req.set_owner(kOwner); // must match the raise
    grpc::ClientContext ctx;
    instr::istate::AlarmAck ack;
    const grpc::Status s = impl_->stub->ClearAlarm(&ctx, req, &ack);
    if (!s.ok())
        spdlog::warn("instrumentation: ClearAlarm(0x{:08x}) failed: {}", static_cast<uint32_t>(id),
                     s.error_message());
}

void Reporter::report_metric(const std::string& name, double value) {
    if (!connected())
        return;
    instr::istate::ReportMetricsRequest req;
    auto* sample = req.add_samples();
    sample->set_name(name);
    sample->set_value(value);
    // `app` is a field on the sample, not on the request -- one batch may
    // carry samples from more than one application.
    sample->set_app(kOwner);
    grpc::ClientContext ctx;
    instr::istate::ReportMetricsReply reply;
    const grpc::Status s = impl_->stub->ReportMetrics(&ctx, req, &reply);
    if (!s.ok())
        spdlog::debug("instrumentation: ReportMetrics({}) failed: {}", name, s.error_message());
}

#else // !ARCHES_WITH_INSTRUMENTATION

// Built without the instrumentation package. This is a supported
// configuration, not a degraded one: an image with ARCHES_INSTRUMENTATION="0"
// runs the client normally and simply reports nothing.

struct Reporter::Impl {};

Reporter::Reporter() : impl_(nullptr) {
    spdlog::debug("instrumentation: not compiled in; alarm and metric reporting disabled");
}
Reporter::~Reporter() = default;
bool Reporter::connected() const { return false; }
void Reporter::register_metrics() {}
void Reporter::raise(AlarmId, const std::string&) {}
void Reporter::clear(AlarmId) {}
void Reporter::report_metric(const std::string&, double) {}

#endif

} // namespace arches::client::instrumentation
