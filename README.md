# Arches-Client

## Project Summary

Arches-Client is the embedded C++17 daemon that makes the Arches hardware-validation stack remotely controllable. `src/main.cpp` and `src/app.cpp` assemble the application; `src/config` handles runtime configuration and identity; `src/hal_service` exposes the client gRPC service; and `src/hal_bridge` serializes HAL results for the server and SDK. `src/run/RunOrchestrator` owns test sequencing and lifecycle, while `src/spool/Spool` supports durable local result handling during disconnects. Diagnostic, logging, configuration, and systemd files support embedded Linux deployment.

The registry under `src/registry` contains adapters for module families declared by the HAL protocol, including CPU, memory, GPIO, storage, networking, camera, display, MCU, and safety interfaces. Registry discovery keeps adapter selection separate from orchestration. `conf/client.conf`, its development variant, and the sample file document runtime inputs; CMake, Make, Docker, and `run.sh` provide build and target workflows.

Focused C++ tests cover adapter registration, application wiring, result serialization, and orchestration. They exercise important seams but do not prove behavior against real device nodes, permissions, transports, or target timing. Statuses below describe repository evidence, not manual hardware verification.

The implementation depends on the shared HAL protobuf version and on target permissions. Review configuration defaults, service startup ordering, spool recovery, adapter availability, and server compatibility as part of release qualification. Also check clean-install behavior, upgrade behavior, interrupted-run recovery, and log rotation on the target. No manual verification is implied by the source-level status values.

## Features

| # | Name / Description | Implemented? | Test Coverage? | Manual Test |
|---:|---|---|---|---|
| 1 | C++17 executable and application wiring | Implemented | `tests/test_app_wiring.cc` | [ ] Manual verification required |
| 2 | Runtime configuration and identity | Implemented | Partial | [ ] Manual verification required |
| 3 | gRPC HAL service | Implemented | Partial: wiring only | [ ] Manual verification required |
| 4 | Result serialization bridge | Implemented | `tests/test_result_serializer.cc` | [ ] Manual verification required |
| 5 | Run orchestration | Implemented | `tests/test_run_orchestrator.cc` | [ ] Manual verification required |
| 6 | Durable result spool | Implemented | Unknown | [ ] Manual verification required |
| 7 | Adapter registry and discovery | Implemented | `tests/test_adapter_registry.cc` | [ ] Manual verification required |
| 8 | Hardware module adapters | Implemented | Partial | [ ] Manual verification required |
| 9 | Diagnostics and logging | Implemented | Unknown | [ ] Manual verification required |
| 10 | Configuration and systemd deployment | Implemented | Unknown | [ ] Manual verification required |

## Validation

`make build`; `make test`; `make lint`; `make format`. Hardware-facing behavior requires a compatible Arches target.

## Existing Documentation

The **gRPC IV&V client** for the Arches project (NVIDIA Jetson target, STM32
co-processor). The client runs on the board under validation and:

1. Drives the hardware through the **Arches-HAL** C++ API (static + dynamic HAL
   libraries), one adapter per HAL module.
2. Speaks the shared **gRPC/protobuf** wire contracts (`client_service.proto`
   streaming up to Arches-Server, `hal_service.proto` for the SDK/E2E path).
3. Spools results durably to SQLite and streams them to Arches-Server.

The implementation is wired to `../Arches-HAL`; the proto packages
(`arches.stream`, `hal.common`, `hal`, `hal.modules.*`) are shared wire
contracts with Arches-Server and Arches-PySDK. The client's own C++ namespace
is `arches::client`, a sibling namespace under `arches::`.

## Architecture

```
┌─ TARGET BOARD (NVIDIA Jetson) ────────────────────────────────────────────┐
│  Hardware (under validation)                                              │
│     ▲                                                                      │
│  Arches-HAL  (C++17, one module per component, extends TestBase)          │
│     ▲ HAL API (lib_base.h: hal::Value / hal::Status / hal::TestResult)    │
│  Arches-Client  ◄── this repo                                             │
│   ├─ AdapterRegistry   (only dispatch point; one adapter per HAL module)   │
│   ├─ adapters/*        (35 real HAL-backed + honest SKIPPED stubs)        │
│   ├─ ResultSerializer  (hal::TestResult  ↔  hal.common.TestResult[Any])   │
│   ├─ RunOrchestrator   (seq/timestamp stamping, backpressure on spool full)│
│   ├─ Spool             (SQLite, schema v2, durable + quarantine)          │
│   ├─ StreamClient      (bidi gRPC → Arches-Server, client_service.proto)   │
│   ├─ HalService        (gRPC endpoint for SDK/E2E, no DB writes)           │
│   ├─ Janitor / LogCollector / DiagSocket / OtaClient                       │
│   └─ config            (TOML/YAML, SQLite-backed where applicable)        │
└─────▲──────────────────────────────────────────────────────────────────────┘
      │ gRPC + FlatBuffers
┌─ x86 LINUX HOST ──────────────────────────────────────────────────────────┐
│  Arches-Server ──GraphQL──► Arches-Dashboard                               │
└────────────────────────────────────────────────────────────────────────────┘
```

### Adapter model (the core of this rewrite)

- `AdapterRegistry::invoke(module_id, mode, cfg)` is the **only** dispatch
  point. There is no `switch(module_id)` anywhere else.
- **35 real adapters** wrap HAL modules that have a C++ implementation
  (`*_adapter.cc` calling the HAL API in `../Arches-HAL/src`).
- **~16 proto-only modules** have no HAL C++ lib yet. They get a generic
  `StubAdapter` that returns an honest `SKIPPED` result naming the module and
  "not yet implemented" — **never a fake pass**.
- A module id that is **unknown** (not in `modules.yaml`, no adapter) returns a
  single `ERROR` result ("no adapter") — never an empty vector, never a crash.
- An adapter that **throws** is caught by the registry and converted to a
  single `ERROR` result ("adapter exception").

`build_adapter_registry()` (in `app.cpp`) enforces: every module in
`modules.yaml` gets an adapter — real if HAL-backed, stub otherwise. No module
is silently dropped. `tests/test_app_wiring.cc` locks this policy.

### Linking strategy (why no local proto generation)

The client does **not** generate any protobuf sources. It consumes the HAL
subproject's pre-built proto library:

- `add_subdirectory(../Arches-HAL ${CMAKE_BINARY_DIR}/arches_hal_build EXCLUDE_FROM_ALL)`
- link **`acu_full_test`** (static — all HAL modules + `lib_base`, threads only)
- link **`hal_protos_shared`** (shared — all generated protos + gRPC; carries
  protobuf + gRPC **transitively** via PUBLIC)

This makes the HAL subproject the single source of generated proto symbols, so
there is no ODR / duplicate-symbol risk and no `find_package(gRPC/Protobuf)` in
the client root. Validated by a build spike before the rewrite.

## Build & test

The Makefile is the single entry point (per SoCcentric §12):

```bash
make build      # configure + ninja (host), 222 targets
make test       # ctest -L arches_client (filters out ~36 unbuilt HAL tests)
make lint       # cppcheck + clang-tidy (best-effort via compile_commands.json)
make installer  # CPack .deb / .rpm / .zip
make doc        # Doxygen (needs doxygen installed)
make run        # run the client locally
```

The full validation gate is `make build && make test && make lint`.

> **Note on `make test`:** the HAL subproject's `enable_testing()` registers
> ~36 HAL test executables that are not built for the client (EXCLUDE_FROM_ALL).
> Running bare `ctest` fails on those. All client tests carry the
> `arches_client` label and the `test` target filters on it.

### Cross-build for Jetson (ARM64 via Docker/QEMU)

```bash
make docker-build    # build ARM64 binaries in the arches Docker image
make docker-test     # run client tests under QEMU
make deploy-a        # scp lib+bin to 192.168.1.3 (Jetson A)
make deploy-b        # scp lib+bin to 192.168.1.13 (Jetson B)
make run-a [app] [args]   # run on Jetson A
```

The Docker container mounts the parent `Arches/` directory so both
`Arches-Client` and `Arches-HAL` are visible (CMakeLists references
`../Arches-HAL/...`).

## Configuration

- `conf/client.conf.sample` — installed to `/etc/arches/client.conf`.
- Runtime state: `/var/lib/arches` (SQLite spool, logs).
- `systemd/arches-client.service` — runs `/usr/bin/arches-client` as user
  `arches`, starts both the ClientService stream (50051) and HalService (50052).

## Tests

| Test | Covers |
|---|---|
| `test_result_serializer` | `hal::TestResult` ↔ proto `TestResult[Any]` round-trip |
| `test_run_orchestrator` | seq/timestamp stamping, spool-full backpressure |
| `test_adapter_registry` | dispatch, missing→ERROR, throw→ERROR, stub→SKIPPED, replace |
| `test_app_wiring` | every `modules.yaml` module gets an adapter; no silent drop |

## Contracts (do not change without approval — §5)

| Contract | Binds | Lives in |
|---|---|---|
| HAL API | HAL ↔ Client | `../Arches-HAL/include`, `lib_base.h` |
| gRPC `.proto` | Client ↔ Server | `../Arches-HAL/protos` (`client_service`, `hal_service`, modules) |
| Proto packages | wire | `arches.stream`, `hal.common`, `hal`, `hal.modules.*` |

The proto packages use native Arches naming because they are the shared wire
contract with Arches-Server and Arches-PySDK. Renaming them further would be a
§5 breaking change. The client's own C++ namespace is `arches::client`.