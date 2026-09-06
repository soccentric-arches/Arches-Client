#pragma once
/**
 * @file src/instrumentation/Instrumentation.h
 * @brief Optional Arches-Instrumentation reporting: alarms and metrics.
 *
 * R5 (Arches-Instrumentation REQ): an alarm is declared by an application and
 * by nothing else. Arches-Client is one of those applications; this is how it
 * declares.
 *
 * EVERYTHING HERE IS OPTIONAL AND NON-FATAL, in two independent senses:
 *
 *   1. Compile time. When the instrumentation package is not installed, this
 *      whole facility compiles to no-ops (ARCHES_WITH_INSTRUMENTATION is not
 *      defined). The client builds and runs identically on a host or an image
 *      without instrumentation, so a developer needs nothing extra installed.
 *
 *   2. Run time. Even when compiled in, a missing or unreachable
 *      /run/instrumentation/application.sock is a normal state, not an error.
 *      Every call degrades to a log line.
 *
 * This is a contract, not a convenience: an application must keep running when
 * instrumentation is down, which is why arches-client.service is `After=` but
 * NOT `Requires=` instr-istate.service (Task.md E-01).
 *
 * See Arches-Instrumentation/docs/application-onboarding.md.
 */

#include <cstdint>
#include <memory>
#include <string>

namespace arches::client::instrumentation {

/// Owner name on every alarm call. Required by R5, and it must be identical on
/// raise and clear -- an alarm raised as one owner and cleared as another does
/// not clear. One constant, used everywhere.
inline constexpr const char* kOwner = "arches-client";

/// Alarm ids allocated to arches-client in the application domain
/// (Arches-Instrumentation/catalog/alarms.yaml, module 0x11).
///
/// These are duplicated from the catalog rather than included from it because
/// the catalog's generated header lives in the instrumentation build tree and
/// this file must compile without it. They are covered by a static_assert-style
/// test in tests/, and the layout is locked, so drift is detectable.
enum AlarmId : uint32_t {
    kRunAborted = 0x41130001u,       ///< client_run_aborted
    kServerStreamLost = 0x41120002u, ///< client_server_stream_lost
    kSpoolBacklog = 0x41140003u,     ///< client_spool_backlog
};

/// @brief Reports alarms and metrics to istate, when it is there.
///
/// Safe to construct and call unconditionally. All methods are no-ops when
/// instrumentation is absent at compile time or unreachable at run time.
class Reporter {
    public:
    /// @brief Connect if possible. Never throws, never blocks past a short deadline.
    Reporter();
    ~Reporter();

    Reporter(const Reporter&) = delete;
    Reporter& operator=(const Reporter&) = delete;

    /// @brief True when a live connection to istate exists.
    bool connected() const;

    /// @brief Register this application's metrics. Call once at startup.
    void register_metrics();

    /// @brief Raise an alarm.
    /// @param id One of AlarmId.
    /// @param detail Free text appended as additional_text.
    void raise(AlarmId id, const std::string& detail);

    /// @brief Clear an alarm previously raised with the same id.
    void clear(AlarmId id);

    /// @brief Report a single metric sample.
    void report_metric(const std::string& name, double value);

    private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace arches::client::instrumentation
