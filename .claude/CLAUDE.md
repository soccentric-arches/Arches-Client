# CLAUDE.md - Arches-Client

Master SoCcentric CLAUDE.md applies. This file adds local knowledge only.

Arches-Client is the gRPC IV&V client for the NVIDIA Jetson target. It drives hardware via the Arches-HAL C++ API (`../Arches-HAL`) and streams results to Arches-Server over the shared gRPC/protobuf contracts, all under native Arches naming: own namespace `arches::client`, proto packages under `arches.*`/`hal.*`.

# Commands

* Build: `make build` (configures + Ninja; host)
* Test: `make test` (runs `ctest -L arches_client`)
* Lint: `make lint` (cppcheck + clang-tidy, best-effort)
* Cross-build: `make docker-build` (ARM64 via QEMU), `make deploy-a` / `deploy-b`
* Full validation gate: `make build && make test && make lint`

# Do

* Link the HAL subproject, do not regenerate protos locally. Build: `add_subdirectory(${CMAKE_SOURCE_DIR}/../Arches-HAL ${CMAKE_BINARY_DIR}/arches_hal_build EXCLUDE_FROM_ALL)` then link `acu_full_test` (static, all HAL modules + lib_base) and `hal_protos_shared` (shared, all generated protos + gRPC; carries protobuf + gRPC transitively via PUBLIC). No `find_package(gRPC/Protobuf)` in the client root, no local `protoc` invocation. This is the single source of generated proto symbols so no ODR / duplicate-symbol risk. Validated by a build spike.
* Run tests with the label filter. `ctest -L arches_client` (or `make test`). The HAL subproject's `enable_testing()` registers ~36 HAL test executables that are not built for the client (EXCLUDE_FROM_ALL); bare `ctest` fails on them. All client tests carry the `arches_client` label via `set_tests_properties(... PROPERTIES LABELS "arches_client")` in `tests/CMakeLists.txt`.
* If the first `ninja` pass dies with `mkdir(arches_hal_build): No such file or directory` near the end, just run `ninja` again. This is a known Ninja quirk with `EXCLUDE_FROM_ALL` globbed subdirectories on a fresh configure; the second pass always completes (observed: stops at ~204/222, then `[5/5]` finishes).
* Use a clean build dir when toggling `BUILD_TESTS`. CMake caches `BUILD_TESTS` in `CMakeCache.txt`; reconfiguring with a different value into a stale cache produces confusing results. `rm -rf build/native` before reconfiguring.
* Keep the proto packages (`arches.stream`, `hal.common`, `hal`, `hal.modules.*`) exactly as-is. They are the shared wire contract with Arches-Server and Arches-PySDK. The `namespace arches { namespace stream { ... } }` forward-decl in `src/stream/StreamClient.h` is part of that contract - keep it. The client's own C++ namespace is also `arches::client`, a sibling namespace under `arches::`. Changing proto packages is a section-5 breaking change requiring approval.
* One adapter per HAL module; `AdapterRegistry::invoke()` is the only dispatch point. No `switch(module_id)` anywhere else. Real adapter for HAL-backed modules, `StubAdapter` (honest SKIPPED) for proto-only modules, ERROR for unknown ids, catch+ERROR for throwing adapters. See `tests/test_adapter_registry.cc` and `tests/test_app_wiring.cc`.
* Use `mkstemp` for temp files in tests, never `std::tmpnam`. tmpnam triggers a dangerous-use linker warning; mkstemp is safe. See `tests/test_app_wiring.cc`.
* Export compile_commands.json. `CMAKE_EXPORT_COMPILE_COMMANDS ON` is set in the root CMakeLists so `make lint` (cppcheck/clang-tidy) works without a CMake custom target. `compile_commands.json` lands in `build/native/`.
* Pack the module-specific Result (`hal.modules.<id>.Result{results:[r]}`) into each `TestResultBatch.sample.payload` Any - section-5-approved wire change. `hal_bridge::pack_module_result` builds it via protobuf reflection against the generated descriptor pool (no per-module headers, one generic path for all 35 implemented modules); falls back to a bare `hal.common.TestResult` when the module Result descriptor is absent (unknown/proto-only module) so no result is silently dropped. The batch `sample.type_url` is set to match. Server + PySDK must consume the module-Result wrapper (unpack `hal.modules.<id>.Result`, read `.results`) - not a bare `hal.common.TestResult` anymore. This is a section-5 contract change approved by the user; record in the porting note and update Server/PySDK in their own change sets.
* Parse module Config `test_names` generically. Adapters call `hal_bridge::resolve_test_names(config_bytes, hal_->getAvailableTests())`, which deserializes the `google.protobuf.Any` carrying `hal.modules.<id>.Config`, reads the `repeated string test_names` field via reflection, clamps to what the HAL offers, and falls back to all available when the config names nothing the HAL has. Lives in `hal_bridge/ResultSerializer.cc`.
* Emit real RunState on every terminal/non-terminal batch. `flush_results`/`flush_synthetic` now take a `RunState` and stamp `batch.state` + upsert the spool run-state: single-shot to COMPLETED (empty HAL to FAILED, HAL exception to FAILED), long-term/event ticks to RUNNING, loop-exit-on-stop to CANCELLED, unknown mode / escaping exception to FAILED. PAUSED is reflected via the spool run-state (surfaced by Hello/StateReport), not by emitting a wire batch (avoids racing the driver thread on batch_seq/stream). `ServerMessage::kRestartRun` is handled in `app.cpp` via `orchestrator_->on_restart()` (stop old, start new if configured).

# Don't

* Don't generate protobuf sources in this repo. That creates a second copy of the proto symbols and causes duplicate-symbol / ODR link errors against `hal_protos_shared`. Always consume the HAL subproject's generated protos.
* Don't run bare `ctest` for validation. It will try to execute ~36 unbuilt HAL tests and exit nonzero. Always `ctest -L arches_client`.
* Don't `find_package(gRPC)` / `find_package(Protobuf)` in the client root CMake. They come transitively from `hal_protos_shared`. Adding local finds re-introduces the duplicate-symbol risk.
* Don't rename the proto packages or the `arches::stream` forward-decl. Section-5 contract - shared with Server + PySDK.
* Don't use `std::tmpnam` in tests. Use `mkstemp` (see Do).
* Don't rely on a `ninja cppcheck` target. The client CMake does not define one. The Makefile `cppcheck`/`lint` targets invoke cppcheck/clang-tidy directly against `compile_commands.json` and degrade gracefully when the tools are not installed.
* Don't sleep a full tick in `run_long_term` after stop. The original `sleep_until(next_tick)` left a stopped run asleep for up to 1 s before emitting the CANCELLED terminal batch. Poll `ctx->stop` in 50 ms increments (matching the pause-poll cadence) so StopRun is acknowledged within ~50 ms. Caught by `LongTermStopSetsCancelledState`; see `run/RunOrchestrator.cc`.
* Don't name `Spool::BatchRow` in tests - `BatchRow` is a free struct at `arches::client` namespace scope (spool/Spool.h:24), not nested in `Spool`. `std::vector<Spool::BatchRow>` fails to parse and the compiler silently degrades the parameter to `const int&`. Use plain `BatchRow` (with `using namespace arches::client;`).

# Validation gate - .claude/validate.sh

```
./.claude/validate.sh quick   # seconds: sibling-HAL presence, adapter-registry sanity, cppcheck/clang-tidy/clang-format
./.claude/validate.sh full    # + build + `ctest -L arches_client`
```

Exit codes follow the fleet contract: `0` pass, `1`-`76` fail, `77` means a check never ran (missing toolchain) - which is not a pass. Runs automatically via the `Stop` hook in `.claude/settings.json`, on demand via `/validate`, and as part of any loop the engine executes here.

Human-owned: the Loop Engine runs this script and never edits it. If a check is wrong, fix it yourself or raise it in the loop's Lessons Learned.

<!-- BEGIN loop-engine -->
## Do's and Don'ts (maintained by the Loop Engine)

*Appended by the Loop Engine on loop close; curated by a human when approving a loop out of `in-review/`. Nothing outside this fence is engine-writable.*

<!-- END loop-engine -->
