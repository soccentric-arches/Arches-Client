#pragma once
// hal_bridge/ResultSerializer.h — canonical conversion between hal::TestResult
// and the hal.common.TestResult protobuf.
//
// This is the ONLY place where hal:: types are mapped to/from proto.
// No adapter, streamer, or runner should perform field-level proto mapping.
//
// Rules:
//  - Enum mapping is exhaustive (no default clause; compiler warns on missing arms).
//  - Value variant uses std::visit for to_proto; TypedValue::value_case() for from_proto.
//  - UNSPECIFIED proto status → hal::Status::ERROR with error_message set.
//  - UNSPECIFIED proto severity → hal::Severity::INFO.
//  - Negative durations are clamped to zero with a [SERIALIZER-CLAMP] log line.
//  - Timestamps are UTC (system_clock epoch = Unix epoch).
//  - Subtests recurse up to kMaxSubtestDepth (default 32); deeper trees are
//    flattened into a single error result.

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "lib_base.h"
#include "common.pb.h"
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/any.pb.h>

namespace arches::client::hal_bridge {

// Maximum recursion depth for subtests. Trees deeper than this limit are
// replaced with a single ERROR result with error_message explaining the cut.
inline constexpr int kMaxSubtestDepth = 32;

// ─── Enum conversion ───────────────────────────────────────────────────────

hal::common::TestStatus  to_proto(hal::Status s);
hal::Status              from_proto(hal::common::TestStatus s);

hal::common::TestSeverity to_proto(hal::Severity s);
hal::Severity             from_proto(hal::common::TestSeverity s);

// Explicit ExecutionMode mapping (proto int value → proto enum).
// Replaces the bare static_cast used in RunOrchestrator::flush_results.
// Throws std::invalid_argument for any value outside [1,4].
hal::common::ExecutionMode execution_mode_from_int(int mode_int);

// ─── Typed value ──────────────────────────────────────────────────────────

hal::common::TypedValue to_proto(const hal::Value& v);
hal::Value              from_proto(const hal::common::TypedValue& v,
                                   const std::string& key = "");
// `key` is only used for the [SERIALIZER-FALLBACK] log message.

// ─── Timestamp / Duration ─────────────────────────────────────────────────

// Timestamp: UTC — system_clock epoch is the Unix epoch.
google::protobuf::Timestamp to_proto(std::chrono::system_clock::time_point tp);
std::chrono::system_clock::time_point from_proto(const google::protobuf::Timestamp& ts);

// Duration: nanosecond resolution. Negative values are clamped to zero.
google::protobuf::Duration to_proto(std::chrono::nanoseconds d);
std::chrono::nanoseconds   from_proto(const google::protobuf::Duration& d);

// ─── Map helpers ──────────────────────────────────────────────────────────

void copy_reason_to_proto(
    const std::map<std::string, std::string>& src,
    google::protobuf::Map<std::string, std::string>* dst);

void copy_value_map_to_proto(
    const std::map<std::string, hal::Value>& src,
    google::protobuf::Map<std::string, hal::common::TypedValue>* dst);

void copy_reason_from_proto(
    const google::protobuf::Map<std::string, std::string>& src,
    std::map<std::string, std::string>& dst);

void copy_value_map_from_proto(
    const google::protobuf::Map<std::string, hal::common::TypedValue>& src,
    std::map<std::string, hal::Value>& dst);

// ─── TestOutput ───────────────────────────────────────────────────────────

hal::common::TestOutput to_proto(const hal::TestOutput& out);
hal::TestOutput         from_proto(const hal::common::TestOutput& out);

// ─── TestResult (recursive) ───────────────────────────────────────────────

// depth is used internally to enforce kMaxSubtestDepth.
hal::common::TestResult to_proto(const hal::TestResult& r, int depth = 0);
hal::TestResult         from_proto(const hal::common::TestResult& r, int depth = 0);

// ─── Module Config parsing ────────────────────────────────────────────────
//
// The server sends a per-module Config message packed in a google.protobuf.Any
// (ConfigureRun.module_config). For every implemented HAL module the Config
// shape is `message Config { repeated string test_names = 1; }`. These helpers
// read the server-selected test names out of the Any so the adapter can run
// only the requested subset (falling back to all-available when empty). They use
// protobuf reflection so they work for every module Config without per-module
// generated headers — only the descriptor pool (populated by hal_protos_shared)
// is consulted.

// Extract the raw test_names from a serialized google.protobuf.Any carrying a
// hal.modules.<X>.Config. Returns empty on any parse failure or empty config.
std::vector<std::string> extract_test_names(const std::vector<uint8_t>& any_bytes);

// Resolve the tests to run for a tick: take the server-selected test_names from
// the serialized Config Any, keep only those the HAL actually offers
// (validate-and-clamp), and fall back to `available` when the config is empty,
// unparsable, or names nothing the HAL knows. This is what every adapter calls.
std::vector<std::string> resolve_test_names(const std::vector<uint8_t>& any_bytes,
                                              const std::vector<std::string>& available);

// ─── Module-specific Result packing (§5-approved contract) ─────────────────
//
// The wire contract (hal_service.proto / client_service.proto comments) states
// that ResultEnvelope.payload and ResultSample.payload carry the module-specific
// `hal.modules.<X>.Result` packed as google.protobuf.Any — NOT a bare
// hal.common.TestResult. For implemented modules that Result is
// `message Result { repeated hal.common.TestResult results = 2; }`.
//
// pack_module_result builds that module Result via reflection (so no per-module
// headers are needed) and packs it into `out`. Returns true on success. Returns
// false (and leaves `out` untouched) when the module Result descriptor is not
// found (unknown/proto-only module) — caller should then fall back to packing a
// bare hal.common.TestResult so no result is ever silently dropped.
bool pack_module_result(const std::string& module_id,
                        const std::vector<hal::TestResult>& results,
                        google::protobuf::Any* out);

// Type URL for a module-specific Result (used to populate ResultSample.type_url /
// ResultEnvelope debug). Matches the Any's type_url after PackFrom.
std::string module_result_type_url(const std::string& module_id);

} // namespace arches::client::hal_bridge
