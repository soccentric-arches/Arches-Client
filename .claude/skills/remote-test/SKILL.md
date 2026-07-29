---
name: remote-test
description: Running Arches-Client against ARM64 — the emulated cross-build test run (make docker-test, ctest under QEMU) and deploying/running real binaries on the physical NVIDIA Jetson targets (make deploy-a/b, run-a/b). Use when validating ARM64 behavior or testing on actual Jetson hardware.
---

# Remote test (cross / on-target)

There are two distinct "not native host" test paths in this repo — they
exercise different things and are not interchangeable.

## 1. Cross-build ctest under QEMU (make docker-test)

```
make docker-test
```

Depends on docker-build (which depends on docker-image), then runs
the same label-filtered CTest suite as native, but ARM64-compiled and executed
under QEMU emulation inside the arches container:

```
ctest -L arches_client --output-on-failure
```

(against build/cross, at /workspace/Arches-Client/build/cross inside the
container). Same rule as native: never bare ctest — the sibling
../Arches-HAL subproject's enable_testing() registers unbuilt HAL tests in
this tree too. This validates that the code builds and passes under ARM64,
but QEMU emulation means it is not proof of real hardware timing/GPIO/CUDA
behavior — for that, use the on-target path below.

## 2. On-target: deploy + run on physical Jetson devices

There is no ctest step on the actual Jetson boards — the Makefile deploys
built binaries and runs them directly over SSH, not through CTest.

```
make deploy        # deploy-a + deploy-b
make deploy-a       # deploy build/cross/{lib,bin} to IP_A via scp
make deploy-b       # deploy build/cross/{lib,bin} to IP_B via scp

make run-a          # run-a depends on deploy-a; runs a binary on IP_A over ssh
make run-b          # same, on IP_B
```

Both run-a/run-b depend on their deploy-* target, which depends on
docker-build — so make run-a alone does a full deploy pipeline: cross
image → cross build → scp lib/bin to the device → ssh-run.

Usage, same convention as local make run:

```
make run-a                     # runs every binary in /tmp/arches-client/bin on IP_A
make run-a test_adapter_registry -- <args>   # runs one binary with args
```

Target device IPs, SSH user, and mount path are defined at the top of the
Makefile: IP_A/IP_B (192.168.1.3 / 192.168.1.13), USER
(nvidia), connecting via sshpass+ssh -o StrictHostKeyChecking=no. This is
a lab-network dev convenience, not a pattern to replicate outside this
Jetson bench setup — don't reuse these credentials or the
StrictHostKeyChecking=no pattern for anything reachable outside the local
test network.

## Choosing between them

- Validating ARM64 compiles/links and passes the test suite quickly → make docker-test.
- Validating actual hardware behavior (real GPIO/CUDA/timing, HAL modules that are stubbed or behave differently under QEMU) → make run-a / make run-b on real Jetson boards.
- make list shows what's currently built in both build/native/bin and build/cross/bin before choosing which to deploy/run.
