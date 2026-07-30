# Arches-Client

gRPC IV&V client running on the Jetson. Drives hardware through the Arches-HAL C++ API and streams
results to Arches-Server.

## How you work here

* Commands: `.claude/commands/` — build, test, lint, validate.
* Write: one adapter per HAL module. `AdapterRegistry::invoke()` is the only dispatch point — no `switch(module_id)` anywhere else.
* Test: always `ctest -L arches_client`. Bare `ctest` tries ~36 unbuilt HAL tests and exits nonzero.

## How you validate

* Gate: `./.claude/validate.sh quick` (seconds) or `full` (adds build + tests). Run it before you claim done.
* Exit codes: `0` pass, `1`-`76` fail, `77` a check never ran. **77 is not a pass.**
* The gate and everything under `.claude/lib`, `hooks`, `commands`, `settings.json` is human-owned. Run it, never edit it.

## Do

* Link the HAL subproject: `add_subdirectory(../Arches-HAL ...)`, then link `acu_full_test` (static) and `hal_protos_shared` (shared, carries protobuf + gRPC transitively).
* Rerun ninja if the first pass dies on `mkdir(arches_hal_build)`. Known EXCLUDE_FROM_ALL quirk on a fresh configure; the second pass always completes.
* `rm -rf build/native` before toggling `BUILD_TESTS` — CMake caches it in `CMakeCache.txt`.
* Real adapter for HAL-backed modules, `StubAdapter` (honest `SKIPPED`) for proto-only ones, `ERROR` for unknown ids, catch-and-`ERROR` for throwing adapters.
* `mkstemp` for temp files in tests. `std::tmpnam` trips a dangerous-use linker warning.
* Pack the module Result (`hal.modules.<id>.Result`) into `TestResultBatch.sample.payload` via `hal_bridge::pack_module_result`, and set `sample.type_url` to match. Server and PySDK consume the wrapper, not a bare `hal.common.TestResult`.
* Parse module Config `test_names` through `hal_bridge::resolve_test_names` — reflection over the `Any`, clamped to what the HAL offers, falling back to all available.
* Stamp a real `RunState` on every batch: single-shot to COMPLETED (empty HAL or exception to FAILED), long-term/event ticks to RUNNING, loop-exit-on-stop to CANCELLED. PAUSED goes to spool run-state, never the wire.

## Don't

* **Don't generate protobuf sources here.** Second copy of the proto symbols, duplicate-symbol/ODR link errors against `hal_protos_shared`.
* **Don't run bare `ctest`.**
* **Don't `find_package(gRPC)` or `find_package(Protobuf)`** in the client root. They arrive transitively; local finds re-introduce the duplicate-symbol risk.
* **Don't rename the proto packages** (`arches.stream`, `hal.common`, `hal`, `hal.modules.*`) or the `arches::stream` forward-decl in `src/stream/StreamClient.h`. Shared contract with Server and PySDK; renaming needs approval.
* **Don't `sleep_until(next_tick)` after stop** in `run_long_term` — it left a stopped run asleep up to 1s before the CANCELLED batch. Poll `ctx->stop` in 50ms steps.
* **Don't write `Spool::BatchRow`.** `BatchRow` is a free struct at `arches::client` scope; the nested form fails to parse and the compiler silently degrades the parameter to `const int&`.

<!-- BEGIN loop-engine -->
## Do's and Don'ts (maintained by the Loop Engine)

Appended by the Loop Engine on loop close; curated by a human when approving a loop out of in-review/. Nothing outside this fence is engine-writable.

<!-- END loop-engine -->
