# Arches-Client

The **gRPC IV&V client** for the Arches project (NVIDIA Jetson target, STM32
co-processor). The client runs on the board under validation and:

1. Drives the hardware through the **Arches-HAL** C++ API (static + dynamic HAL
   libraries), one adapter per HAL module.
2. Speaks the shared **gRPC/protobuf** wire contracts (`client_service.proto`
   streaming up to Arches-Server, `hal_service.proto` for the SDK/E2E path).
3. Spools results durably to SQLite and streams them to Arches-Server.

This is the **Arches-native** rewrite of the client. The implementation is wired
to `../Arches-HAL`; the proto packages (`acadia.stream`, `hal.common`, `hal`,
`hal.modules.*`) are shared wire contracts with Arches-Server and Arches-PySDK
and are intentionally **unchanged** — only the client's own C++ namespace was
renamed to `arches::client`.

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
| Proto packages | wire | `acadia.stream`, `hal.common`, `hal`, `hal.modules.*` — **unchanged** |

The proto packages keep the `acadia.*` names because they are the shared wire
contract with Arches-Server and Arches-PySDK. Renaming them would be a §5
breaking change. Only the client's own C++ namespace is `arches::client`.