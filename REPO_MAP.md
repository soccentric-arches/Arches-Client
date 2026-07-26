# Arches-Client

**Remote:** `git@github.com:SiliconCentric/Arches-Client.git`
**Parent project:** [Arches](../Arches.md) (target hardware: **NVIDIA Jetson** (Tegra family, L4T/JetPack OS) application processor with an **STM32** real-time co-processor.)
**Leaf type:** `<Project>-Client`
**Status:** REAL — actively built out

The **gRPC IV&V client** for the Arches project (NVIDIA Jetson target, STM32 co-processor). Runs on the board under validation and:

1. Drives hardware through the **Arches-HAL** C++ API (static + dynamic libraries), one adapter per HAL module.
2. Speaks the shared gRPC/protobuf wire contracts (`client_service.proto` streaming to `Arches-Server`, `hal_service.proto` for the SDK/E2E path).
3. Spools results durably to SQLite and streams them to `Arches-Server`.

**Adapter model:** `AdapterRegistry::invoke(module_id, mode, cfg)` is the only dispatch point (no `switch(module_id)` anywhere else) — 35 real HAL-backed adapters, ~16 proto-only modules get a generic `StubAdapter` returning honest `SKIPPED`, unknown module ids return a single `ERROR` result, and an adapter that throws is caught and converted to `ERROR` (never a crash or empty vector). `build_adapter_registry()` enforces every module in `modules.yaml` gets an adapter — locked by `tests/test_app_wiring.cc`.

**Linking strategy:** the client does **not** generate its own protobuf sources — it links the HAL subproject's pre-built proto library (`add_subdirectory(../Arches-HAL ...)`, links `acu_full_test` static + `hal_protos_shared`), making HAL the single source of generated proto symbols (no ODR risk, no `find_package(gRPC/Protobuf)` in the client root).

**Proto packages** (`arches.stream`, `hal.common`, `hal`, `hal.modules.*`) are native Arches naming — they're a shared wire contract with `Arches-Server`/`Arches-PySDK`; the client's own C++ namespace is `arches::client`, a sibling namespace under `arches::`. Renaming the proto packages further would be a breaking change requiring cross-team approval.

**Build:** single Makefile entry point — `make build` (configure + ninja, 222 targets), `make test` (ctest filtered to the `arches_client` label — the HAL subproject registers ~36 unbuilt HAL tests that a bare `ctest` would fail on), `make lint` (cppcheck + clang-tidy), `make installer` (CPack .deb/.rpm/.zip), `make doc` (Doxygen), `make run`. Full validation gate: `make build && make test && make lint`.

**Cross-build for Jetson:** `make docker-build`/`docker-test` (ARM64 via Docker/QEMU), `make deploy-a`/`deploy-b` (scp to Jetson boards at `192.168.1.3`/`192.168.1.13`), `make run-a [app] [args]`.

**Config/runtime:** `/etc/arches/client.conf` (from `conf/client.conf.sample`), state in `/var/lib/arches` (SQLite spool, logs), systemd unit `systemd/arches-client.service` runs both the ClientService stream (50051) and HalService (50052) as user `arches`.

**Depends on:** `Arches-HAL` (sibling, linked directly, single source of proto symbols), `Arches-Server` (gRPC peer), `Arches-PySDK` (shares wire contract, not yet implemented).

---
Part of the `Arches` project in the `project-managers` fleet. See `docs/project-managers.md` for the full 9-project / 8-leaf hierarchy and shared conventions.
