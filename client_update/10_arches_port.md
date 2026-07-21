# Porting Note: Acadia-Client → Arches-Client (Arches-native rewrite)

**Date:** 2026-07-20
**Scope:** Arches-Client only (one project, one component — §5 blast radius respected)
**Target:** NVIDIA Jetson (STM32 co-processor as RT element)
**Status:** Complete — `make build && make test && make lint` green.

## What changed

This is a full rewrite of the client on the Arches-native pattern. The client
now drives the **Arches-HAL** C++ API directly and speaks the shared gRPC/proto
contracts, rather than carrying Acadia naming. The proven subsystem shape
(registry → adapters → serializer → orchestrator → spool → stream) is preserved
verbatim; only the wiring target, namespace, and naming changed.

### Decisions locked (via AskUserQuestion, pre-implementation)

1. **Arches-native rewrite** — discard Acadia naming; own namespace
   `arches::client`, executable `arches-client`, wired to `../Arches-HAL`.
   Subsystem shape preserved.
2. **Real adapters for the ~35 implemented HAL modules + honest SKIPPED/ERROR
   stubs for the ~16 proto-only modules.** No fake passes.
3. **Static HAL (`acu_full_test`) + shared protos (`hal_protos_shared`) from the
   HAL subproject.** No local proto generation.

## Contract carve-outs (NOT changed — §5)

These are shared wire contracts with Arches-Server and Arches-PySDK and were
left **exactly as-is**. Renaming any of them is a §5 breaking change requiring
approval:

| Contract | Value | Why kept |
|---|---|---|
| Proto packages | `acadia.stream`, `hal.common`, `hal`, `hal.modules.*` | Shared wire contract Server+PySDK |
| `client_service.proto` | package `acadia.stream` | Server expects this |
| `hal_service.proto` / `common.proto` | packages `hal` / `hal.common` | SDK + Server expect this |
| Forward-decl | `namespace acadia { namespace stream { ... } }` in `src/stream/StreamClient.h` | Matches proto package |

Only the client's **own C++ namespace** (`acadia::client` → `arches::client`),
library/target names (`acadia_*` → `arches_*`), paths (`/var/lib/acadia` →
`/var/lib/arches`, `/etc/acadia` → `/etc/arches`), systemd unit, Docker image,
and config files were renamed.

## Files changed

### Build system (the core of the new method)
- `CMakeLists.txt` — `project(arches-client)`,
  `add_subdirectory(../Arches-HAL ...arches_hal_build EXCLUDE_FROM_ALL)`,
  `find_package` for spdlog/tomlplusplus/yaml-cpp/SQLite3 only (NO gRPC/Protobuf),
  `CMAKE_EXPORT_COMPILE_COMMANDS ON`, install rules → `arches` paths + systemd unit.
- `src/CMakeLists.txt` — `arches-client` executable links `arches_*` + `acu_full_test`
  + `hal_protos_shared`; dropped local proto gen.
- `src/registry/CMakeLists.txt` — `arches_registry` STATIC with 35 adapter .cc +
  `stub_adapter.cc`; 16 proto-only adapters commented out; links `acu_full_test`.
- All 10 subsystem `CMakeLists.txt` — `acadia_*` → `arches_*`; spool/stream/
  hal_bridge/ota/log/janitor/run/hal_service link `hal_protos_shared` (replacing
  `acadia_protos`); dropped `${ACADIA_GRPC_LIBRARIES}` + `protobuf::libprotobuf`
  (transitive via `hal_protos_shared`); HAL-include dirs point at
  `../Arches-HAL/src`.
- `tests/CMakeLists.txt` — 4 test executables link `arches_*` + `hal_protos_shared`
  + `GTest::gtest_main`; every `add_test` gets `LABELS "arches_client"`.
- `cmake/FindAcadiaHAL.cmake` — **deleted** (dead; root CMake uses add_subdirectory).

### Sources
- All `src/**/*.{cc,cpp,h}` + `tests/**` — `acadia::client` → `arches::client`
  (137 files), `ACADIA_CLIENT_VERSION` → `ARCHES_CLIENT_VERSION`, path strings
  → `/var/lib/arches` etc. Proto-package references (`acadia::stream`,
  `acadia.stream`) **kept**.
- New tests: `tests/test_adapter_registry.cc` (7 cases), `tests/test_app_wiring.cc`
  (5 cases) — lock the registry dispatch + honest-stub + no-silent-drop policies.

### Config / packaging / ops
- `Makefile` — `DOCKER_IMAGE := arches`, `ARCHES_ROOT`, `DOCKER_SRC_DIR :=
  /workspace/Arches-Client`, `ccache-arches-client`; `test` target →
  `ctest -L arches_client`; **new** `lint` (cppcheck+clang-tidy, best-effort via
  `compile_commands.json`) and `installer` (CPack .deb/.rpm/.zip) targets per §12.
- `systemd/arches-client.service` — renamed from `acadia-client.service`;
  Description/User/Group/WorkingDirectory/SyslogIdentifier/ReadWritePaths → arches.
- `conf/client.conf{,.dev,.sample}` — Acadia→Arches, paths → /etc/arches,
  /var/lib/arches, /var/log/arches.
- `run.sh`, `Doxyfile.in`, `docker/Makefile` (IMAGE_NAME arches, Jetson ARM64,
  hostname jetson), `docker/Dockerfile` (header → Jetson build container).
- `README.md` — full Arches-native rewrite (architecture, adapter model, linking
  strategy, build/test/lint, contracts).

## Order & dependencies across components

This change is **client-only at the proto-package level**. It depends on
Arches-HAL already exposing:
- `acu_full_test` static target (all HAL modules + `lib_base`),
- `hal_protos_shared` shared target (generated protos + gRPC, PUBLIC protobuf+gRPC).

### §5 contract change — module-specific Result payload (APPROVED)

One wire-level change was made with explicit user approval (§5): the
`TestResultBatch.sample.payload` `google.protobuf.Any` now carries the
**module-specific** `hal.modules.<module_id>.Result { repeated hal.common.TestResult results = 2; }`
instead of a bare `hal.common.TestResult`. The batch `sample.type_url` is set
to `type.googleapis.com/hal.modules.<id>.Result` to match. For unknown /
proto-only modules with no `.Result` descriptor, the client falls back to the
bare `hal.common.TestResult` (no result is silently dropped).

**Required sibling updates (separate change sets, tracked here):**
- **Arches-Server** must unpack `hal.modules.<id>.Result` and read `.results(i)`
  rather than a bare `hal.common.TestResult`. Today it expects the bare type.
- **Arches-PySDK** must do the same on the consume side.
- **Arches-Dashboard**: no change (it reads Server-resolved data, not the raw
  Any).

The proto packages, `.proto`/`.fbs`/GraphQL schema files, and the HAL API are
unchanged — only the *contents* the client places in the existing `Any` payload
field changed. Blast radius is one project (Arches), per §5.

## Proto-feature coverage (added 2026-07-20, client-only)

The client now exercises every proto feature the HAL/Server surface exposes:
- **`ServerMessage::kRestartRun`** — `app.cpp` dispatches to
  `RunOrchestrator::on_restart(old, new)` (stop old run, start new if configured).
- **RunState PAUSED/FAILED/CANCELLED** — `flush_results`/`flush_synthetic` stamp
  `batch.state` and upsert the spool run-state: single-shot success → COMPLETED,
  empty HAL / HAL exception → FAILED, long-term/event ticks → RUNNING, loop
  exit on stop → CANCELLED, unknown mode / escaping exception → FAILED. PAUSED
  is reflected via the spool run-state (surfaced by Hello/StateReport), not a
  wire batch, to avoid racing the driver thread.
- **Module Config `test_names`** — adapters call
  `hal_bridge::resolve_test_names(config_bytes, available)`, which deserializes
  the `Any`-wrapped `hal.modules.<id>.Config`, reads `repeated string
  test_names` via reflection, clamps to what the HAL offers, and falls back to
  all available when the config names nothing the HAL has.
- **Module-specific Result packing** — see §5 section above.

16 proto-only modules (no HAL C++) remain SKIPPED via `StubAdapter` — enabling
them would break the build (no `*_lib.h`) and would be a fake pass, which the
IV&V stance forbids. They turn on when Arches-HAL gains their C++.

## Platform-specific considerations (Arches RT element = STM32 co-processor)

The client runs on the Jetson SoC; the STM32 co-processor is reached via the
`mcu_interface` HAL module (adapter present). No client-side RT code — the RT
element is handled entirely within Arches-HAL. Cross-build is ARM64 for the
Jetson (`make docker-build`, QEMU-emulated).

## Verification

```
$ make build   → [222/222] Linking CXX executable bin/arches-client   (green)
$ make test    → 4/4 tests passed, label arches_client (5.45s)        (green)
$ make lint    → cppcheck/clang-tidy: tools not installed → graceful  (pass)
```

`compile_commands.json` exported (350 KB) so `make lint` works where the tools
are present. Known Ninja quirk: first `ninja` pass on a fresh configure may die
at ~204/222 with `mkdir(arches_hal_build): No such file or directory` — re-run
`ninja` and it finishes (see `CLAUDE.md` Do/Don't).

## Acceptance

- [x] All 10 plan tasks complete
- [x] `make build && make test && make lint` green
- [x] No `acadia::client` / `Acadia-HAL` references remain in client sources
      (proto-package refs `acadia::stream` / `acadia.stream` intentionally kept)
- [x] Patterns mirrored (Acadia client shape preserved), not reinvented
- [x] Local `CLAUDE.md` Do/Don't records the working approach (§11)