# Arches-Client — specs

Remote: `git@github.com:SiliconCentric/Arches-Client.git`
Parent project: Arches (target hardware: NVIDIA Jetson, Tegra family, L4T/JetPack OS, with an STM32 real-time co-processor)
Leaf type: `<Project>-Client`
Status: REAL — actively built out

## What this is

Arches-Client is the gRPC IV&V client for the NVIDIA Jetson target. It drives hardware via the Arches-HAL C++ API (`../Arches-HAL`) and streams results to Arches-Server over the shared gRPC/protobuf contracts, all under native Arches naming: own namespace `arches::client`, proto packages under `arches.*`/`hal.*`.

## Architecture notes

* Links the HAL subproject at build time rather than regenerating protos — HAL is the single source of generated proto symbols, so there is no ODR/duplicate-symbol risk.
* One adapter per HAL module; `AdapterRegistry::invoke()` is the only dispatch point. Real adapters for HAL-backed modules, a `StubAdapter` (honest `SKIPPED`) for proto-only modules.
* The proto packages (`arches.stream`, `hal.common`, `hal`, `hal.modules.*`) are a shared wire contract with Arches-Server and Arches-PySDK — a section-5 breaking change requiring approval to rename.
* Module results pack into `hal.modules.<id>.Result` via protobuf reflection (`hal_bridge::pack_module_result`), with a bare `hal.common.TestResult` fallback for unknown/proto-only modules — Server and PySDK must consume the wrapper form.
* Module `Config` `test_names` are parsed generically via `hal_bridge::resolve_test_names`, deserializing the `google.protobuf.Any` payload and clamping to what the HAL offers.
* Every terminal/non-terminal batch stamps a real `RunState`, upserted into the spool run-state (single-shot → COMPLETED/FAILED, long-term/event ticks → RUNNING, loop-exit-on-stop → CANCELLED, unknown mode/exception → FAILED). PAUSED is reflected via spool state, not a wire batch.
