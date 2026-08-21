# Arches-Client Build and Deployment

## 1. Purpose and scope

Arches-Client is the C++ test/client application set built with CMake and Ninja. It consumes the sibling `Arches-HAL` source and shared protocol definitions. The Makefile supports native host builds and ARM64 Docker/QEMU builds for NVIDIA Jetson targets.

## 2. Prerequisites

Install a C++17 compiler, CMake, Ninja, the project CMake dependencies, CPack, and (for checks) CTest, `cppcheck`, `clang-tidy`, and optionally `cmake-format`. Native builds expect `../Arches-HAL` beside this repository. Docker is required for cross builds; ARM64 execution also requires QEMU user emulation as provided by the Docker setup. SSH/SCP and `sshpass` are required for deployment.

## 3. Setup

From this repository root, inspect available applications with `make list`. Native configuration is created by the Makefile, so no separate configure script is required. For ARM64 work, `make docker-install` and `make docker-image` install/prepare the Docker toolchain. These targets may use `sudo` when Docker is absent.

## 4. Build and compile

- `make configure` configures `build/native` with Ninja and tests enabled.
- `make build` builds the native client; `make` is an alias for this path.
- `make docker-configure` configures the ARM64 cross build.
- `make docker-build` builds `build/cross` with Ninja through the project Docker image.
- `make list` lists binaries produced by either native or cross builds.

## 5. Test and lint

- `make test` builds as needed, then runs `ctest -L arches_client --output-on-failure` in `build/native`.
- `make docker-test` runs the same client-labeled tests under ARM64 Docker/QEMU.
- `make lint` runs best-effort `cppcheck` and `clang-tidy`; missing tools or compile commands are reported and can cause checks to be skipped.
- `make format` runs the generated Ninja format target and `cmake-format` where installed.
- `make doc` generates configured documentation when the required documentation toolchain is available.

## 6. Packaging and artifacts

`make installer` builds and invokes CPack. The Makefile advertises `.deb`, `.rpm`, and `.ipk` packages. Native artifacts are under `build/native`; cross binaries and libraries are under `build/cross`.

## 7. Deployment, install, and run

- `make deploy-a` deploys cross-built `lib` and `bin` directories to `192.168.1.3`.
- `make deploy-b` deploys them to `192.168.1.13`.
- `make deploy` runs both deployments.
- `make run` runs local test applications; pass an application name and arguments as supported by the Makefile, or omit them to run all.
- `make run-a` and `make run-b` deploy then run remotely; these require reachable target hardware, SSH access, and the cross build.

The default remote credentials are defined by the Makefile and must not be treated as production credentials. Override the target addresses and SSH variables on the command line or provide the repository's documented environment overrides. Deployment and remote execution are conditional on hardware/network access.

## 8. Cleanup

`make clean` removes `build`. `make docker-clean` removes the Docker cross-build directory. `make docker-shell` opens a development shell in the cross image.

## 9. Hardware, cloud, and manual verification caveats

No cloud deployment is defined by this Makefile. Native tests do not prove Jetson behavior; use `make docker-test` and then `make run-a`/`make run-b` on named, reachable devices for hardware verification. Confirm the target board, IP, credentials, and installed runtime dependencies before deployment.
