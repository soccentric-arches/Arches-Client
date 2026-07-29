---
name: native-test
description: Running Arches-Client's unit/integration test suite on the local (host) build via ctest — make test, the arches_client label filter, and individual test binaries in build/native/bin. Use when running or debugging tests on the local development machine.
---

# Native test (local ctest)

```
make test
```

This depends on `build` (so it always rebuilds first), then runs:

```
cd build/native && ctest -L arches_client --output-on-failure
```

## Why the -L arches_client label filter is mandatory

The linked ../Arches-HAL subproject calls enable_testing(), which
registers its ~36 test executables in the same CMake build tree — but they're
EXCLUDE_FROM_ALL and never actually get built here. Never run bare
ctest in build/native; it will enumerate those unbuilt HAL tests, fail
to find their binaries, and report spurious failures that have nothing to do
with this repo. Every Arches-Client test is tagged with
set_tests_properties(<test> PROPERTIES LABELS "arches_client") in
tests/CMakeLists.txt specifically so the label filter selects only this
repo's tests.

## Running a subset

From `build/native`, standard ctest filtering flags work on top of the label:

```
ctest -L arches_client --output-on-failure -R <test-name-regex>
ctest -L arches_client --output-on-failure -V          # verbose, all output
```

## Running a single test binary directly

Built test executables land in build/native/bin/ alongside the app binary
(arches-client), e.g. test_adapter_registry, test_app_wiring,
test_result_serializer, test_run_orchestrator. Run one directly for fast
iteration without going through CTest:

```
./build/native/bin/test_adapter_registry
```

make run (see the build skill) can also invoke any binary in
build/native/bin with LD_LIBRARY_PATH set correctly, including test
binaries: make run test_adapter_registry.

## Gotchas from repo CLAUDE.md

- Use mkstemp, never std::tmpnam, for temp files inside tests — tmpnam
  triggers a dangerous-use linker warning.
- BatchRow is a free struct in the arches::client namespace
  (spool/Spool.h), not nested under Spool — std::vector<Spool::BatchRow>
  silently parses wrong (degrades a parameter to const int&) instead of
  failing to compile. Use plain BatchRow with using namespace arches::client;.
