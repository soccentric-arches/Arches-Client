# Arches-Client — specs

Remote: `git@github.com:SiliconCentric/Arches-Client.git`
Target: NVIDIA Jetson (Tegra, L4T/JetPack). Namespace `arches::client`; proto packages under `arches.*` / `hal.*`.

The gRPC IV&V client. Drives hardware via the Arches-HAL C++ API and streams results to
Arches-Server over the shared gRPC/protobuf contracts.

## Architecture

* **HAL linking** — links the HAL subproject at build time instead of regenerating protos. HAL is the single source of generated proto symbols, so there is no ODR/duplicate-symbol risk.
* **Adapter registry** — one adapter per HAL module; `AdapterRegistry::invoke()` is the only dispatch point. Real adapters for HAL-backed modules, `StubAdapter` returning an honest `SKIPPED` for proto-only ones.
* **Wire contract** — `arches.stream`, `hal.common`, `hal`, `hal.modules.*` are shared with Arches-Server and Arches-PySDK. Renaming any of them is a coordinated breaking change.
* **Result packing** — module results pack into `hal.modules.<id>.Result` via protobuf reflection (`hal_bridge::pack_module_result`), falling back to a bare `hal.common.TestResult` when the module descriptor is absent.
* **Config parsing** — `hal_bridge::resolve_test_names` deserializes the `google.protobuf.Any` carrying `hal.modules.<id>.Config`, reads `test_names` by reflection, and clamps to the HAL's available tests.
* **Run state** — every batch carries a real `RunState`, upserted into the spool run-state. PAUSED is reflected in spool state only, never emitted as a wire batch. `ServerMessage::kRestartRun` is handled in `app.cpp` via `orchestrator_->on_restart()`.
* **Spool** — `BatchRow` is a free struct at `arches::client` namespace scope (`spool/Spool.h`), not nested in `Spool`.
