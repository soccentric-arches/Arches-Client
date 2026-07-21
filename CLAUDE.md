# CLAUDE.md — Arches-Client

Master SoCcentric CLAUDE.md applies. This file adds local knowledge only.

Arches-Client is the gRPC IV&V client for the NVIDIA Jetson target. It drives
hardware via the Arches-HAL C++ API (`../Arches-HAL`) and streams results to
Arches-Server over the shared gRPC/protobuf contracts. This is the Arches-native
rewrite: own namespace `arches::client`, proto packages deliberately unchanged.

- Build: `make build` (configures + Ninja; host)
- Test: `make test` (runs `ctest -L arches_client`)
- Lint: `make lint` (cppcheck + clang-tidy, best-effort)
- Cross-build: `make docker-build` (ARM64 via QEMU), `make deploy-a` / `deploy-b`
- Full validation gate: `make build && make test && make lint`

## Do

- **Link the HAL subproject, do not regenerate protos locally.** Build:
  `add_subdirectory(${CMAKE_SOURCE_DIR}/../Arches-HAL ${CMAKE_BINARY_DIR}/arches_hal_build EXCLUDE_FROM_ALL)`
  then link `acu_full_test` (static, all HAL modules + lib_base) and
  `hal_protos_shared` (shared, all generated protos + gRPC; carries protobuf +
  gRPC transitively via PUBLIC). No `find_package(gRPC/Protobuf)` in the client
  root, no local `protoc` invocation. This is the single source of generated
  proto symbols → no ODR / duplicate-symbol risk. Validated by a build spike.
- **Run tests with the label filter.** `ctest -L arches_client` (or `make test`).
  The HAL subproject's `enable_testing()` registers ~36 HAL test executables
  that are not built for the client (EXCLUDE_FROM_ALL); bare `ctest` fails on
  them. All client tests carry the `arches_client` label via
  `set_tests_properties(... PROPERTIES LABELS "arches_client")` in
  `tests/CMakeLists.txt`.
- **If the first `ninja` pass dies with `mkdir(arches_hal_build): No such file or
  directory` near the end, just run `ninja` again.** This is a known Ninja quirk
  with `EXCLUDE_FROM_ALL` globbed subdirectories on a fresh configure; the second
  pass always completes (observed: stops at ~204/222, then `[5/5]` finishes).
- **Use a clean build dir when toggling `BUILD_TESTS`.** CMake caches
  `BUILD_TESTS` in `CMakeCache.txt`; reconfiguring with a different value into a
  stale cache produces confusing results. `rm -rf build/native` before
  reconfiguring.
- **Keep the proto packages (`acadia.stream`, `hal.common`, `hal`, `hal.modules.*`)
  exactly as-is.** They are the shared wire contract with Arches-Server and
  Arches-PySDK. The `namespace acadia { namespace stream { ... } }` forward-decl
  in `src/stream/StreamClient.h` is part of that contract — keep it. Only the
  client's own C++ namespace is `arches::client`. Changing proto packages is a §5
  breaking change requiring approval.
- **One adapter per HAL module; `AdapterRegistry::invoke()` is the only dispatch
  point.** No `switch(module_id)` anywhere else. Real adapter for HAL-backed
  modules, `StubAdapter` (honest SKIPPED) for proto-only modules, ERROR for
  unknown ids, catch+ERROR for throwing adapters. See
  `tests/test_adapter_registry.cc` and `tests/test_app_wiring.cc`.
- **Use `mkstemp` for temp files in tests, never `std::tmpnam`.** tmpnam triggers
  a dangerous-use linker warning; mkstemp is safe. See `tests/test_app_wiring.cc`.
- **Export compile_commands.json.** `CMAKE_EXPORT_COMPILE_COMMANDS ON` is set in
  the root CMakeLists so `make lint` (cppcheck/clang-tidy) works without a CMake
  custom target. `compile_commands.json` lands in `build/native/`.

## Don't

- **Don't generate protobuf sources in this repo.** That creates a second copy of
  the proto symbols and causes duplicate-symbol / ODR link errors against
  `hal_protos_shared`. Always consume the HAL subproject's generated protos.
- **Don't run bare `ctest` for validation.** It will try to execute ~36 unbuilt
  HAL tests and exit nonzero. Always `ctest -L arches_client`.
- **Don't `find_package(gRPC)` / `find_package(Protobuf)` in the client root
  CMake.** They come transitively from `hal_protos_shared`. Adding local finds
  re-introduces the duplicate-symbol risk.
- **Don't rename the proto packages or the `acadia::stream` forward-decl.** §5
  contract — shared with Server + PySDK.
- **Don't use `std::tmpnam` in tests.** Use `mkstemp` (see Do).
- **Don't rely on a `ninja cppcheck` target.** The client CMake does not define
  one. The Makefile `cppcheck`/`lint` targets invoke cppcheck/clang-tidy directly
  against `compile_commands.json` and degrade gracefully when the tools are not
  installed.