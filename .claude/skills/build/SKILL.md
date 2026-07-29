---
name: build
description: Building Arches-Client — both the native (host) build and the ARM64 cross build (Docker/QEMU) for the NVIDIA Jetson target, covering every build-related Makefile target and option. Use when compiling, reconfiguring, cleaning, packaging, or debugging a build in this repo.
---

# Build (native + cross)

Arches-Client has two independent build trees, kept in separate directories so
their CMake caches never collide:

| Build       | Directory        | Arch          | Driven by                     |
|-------------|-------------------|---------------|--------------------------------|
| Native      | `build/native`    | host (`uname -m`) | `cmake -GNinja` + `ninja` directly |
| Cross       | `build/cross`     | `linux/arm64` | Docker (QEMU) running the same cmake/ninja inside the container |

Both link the sibling `../Arches-HAL` subproject (static `acu_full_test` +
shared `hal_protos_shared`) via `add_subdirectory(... EXCLUDE_FROM_ALL)`, so
`../Arches-HAL` must exist as a sibling checkout before either build works.

## Native build targets

- `make configure` — `mkdir -p build/native && cmake -GNinja -DBUILD_TESTS=ON .` into `build/native`.
- `make build` (default, `make`/`make all`) — depends on `configure`, then `ninja -j$(JOBS)`. `JOBS` defaults to `nproc`; override with `make build JOBS=4`.
- `make run` — runs a built test app locally with `LD_LIBRARY_PATH=build/native/lib`. Usage: `make run [name] [args...]`; no name runs every binary in `build/native/bin`. Prefix with `V=1` to echo the exact command.
- `make installer` — depends on `build`; packages via CPack (`cpack -G DEB`, `-G RPM`, `-G ZIP`) from `build/native`.
- `make clean` — `rm -rf build` (removes **both** native and cross trees; there is no per-tree clean target).
- `make list` — lists binaries present in `build/native/bin` and `build/cross/bin` side by side.

Test and lint targets (`make test`, `make lint`, `make cppcheck`, `make format`) also live under the native tree — see the `native-test` and `validate` skills for those.

## Cross build targets (Docker, ARM64 for Jetson)

- `make docker-install` — installs `docker.io` via apt if `docker` is not on PATH.
- `make docker-image` — depends on `docker-install`; builds the `arches` image (`make -C docker build`, i.e. `docker buildx build --platform linux/arm64 --load ...`) only if it doesn't already exist. Force a rebuild with `docker rmi arches` first (or `make -C docker clean`).
- `make docker-configure` — depends on `docker-image`; runs `cmake -GNinja -DBUILD_TESTS=ON` inside the container at `/workspace/Arches-Client/build/cross`.
- `make docker-build` — depends on `docker-image`; configures if `build.ninja` is missing, then `ninja -j$(DOCKER_JOBS)` inside the container. `DOCKER_JOBS` is capped at 4 (never `nproc`) — QEMU can't sustain more parallel `cc1plus` processes without segfaulting.
- `make docker-clean` — `rm -rf build/cross` inside the container.
- `make docker-shell` — interactive `bash` inside the `arches` container for ad-hoc debugging, with the same volume mounts as the build targets.

Docker run mechanics common to all `docker-*` build targets:
- Mounts the **parent** `Arches/` directory (not just this repo) to `/workspace`, so `../Arches-HAL` resolves inside the container — `ARCHES_ROOT := $(dirname $(realpath .))`.
- Mounts a named volume `ccache-arches-client:/home/rpi/.ccache` to persist compiler cache across runs.
- Uses `-t --init` so Ctrl-C reaches the containerized `ninja` process and tini reaps children correctly.

`docker/Makefile` (invoked via `make -C docker <target>`) is the lower-level image builder: `build` (buildx, passes host `UID`/`GID`), `run` (interactive container, no `--gpus` since GPU tests are on-target only), `clean` (`docker rmi arches`), `delete-images` (removes **all** local Docker images — destructive, don't run casually).

## Known build quirks (see repo `CLAUDE.md`)

- If the first `ninja` pass in either tree dies near the end with `mkdir(arches_hal_build): No such file or directory`, just re-run `ninja` (or re-run `make build`/`make docker-build`). This is a known Ninja quirk with `EXCLUDE_FROM_ALL` globbed subdirectories on a fresh configure; the second pass always completes.
- Toggling `BUILD_TESTS` (or any cached CMake option) into an existing `build/native` or `build/cross` directory can silently keep the old cached value. `rm -rf build/native` (or `build/cross`) before reconfiguring with a different value, rather than trusting an in-place reconfigure.
- Never `find_package(gRPC)`/`find_package(Protobuf)` and never regenerate `.proto` sources in this repo's own CMake — both protobuf/gRPC and all generated proto symbols arrive transitively from `hal_protos_shared` in `../Arches-HAL`. Adding either reintroduces ODR/duplicate-symbol link errors.
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` is already set at the root, so `build/native/compile_commands.json` exists after any `configure`/`build` — no extra target needed for `cppcheck`/`clang-tidy`.
