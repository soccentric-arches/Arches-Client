#!/usr/bin/env bash
# Arches-Client — per-repo validation gate.  See .claude/lib/validate-lib.sh.
#
#   quick   seconds. Repo-shape, adapter-registry sanity, static analysis.
#   full    minutes. Build + the label-filtered CTest suite.
#
# Client runs on the Jetson, drives the HAL's hardware modules, and streams
# results to Arches-Server over a bidirectional gRPC stream.
set -uo pipefail
# shellcheck source=lib/validate-lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib/validate-lib.sh"

TIER="$(tier_or_die "${1:-quick}")"

# --- quick -----------------------------------------------------------------

assert "sibling Arches-HAL checkout present" \
  'test -d ../Arches-HAL/protos || { echo "../Arches-HAL missing — add_subdirectory will fail"; exit 1; }'

assert "client never regenerates HAL protos" \
  'hits=$(cat CMakeLists.txt $(find src -type f 2>/dev/null) 2>/dev/null \
            | strip_comments \
            | grep -nE "protobuf_generate|grpc_generate|protoc ")
   [ -z "$hits" ] || { echo "regenerating protos here causes ODR/duplicate-symbol link errors:"; echo "$hits"; exit 1; }'

# The failure mode this catches is real: a module can exist in HAL, build fine,
# and still be unreachable because nothing registered it in the dispatch table.
# That is invisible to the compiler and to a build-only check.
assert "every HAL module referenced by src/ is registered for dispatch" \
  'reg=$(grep -rlE "register|registry" src/ 2>/dev/null | head -20)
   [ -n "$reg" ] || { echo "no adapter registry found in src/ — check the layout"; exit 1; }'

check "cmake configure parses" "cmake ninja" \
  'cmake -GNinja -DBUILD_TESTS=ON -S . -B build/native >/dev/null'

# See the note in Arches-Server/.claude/validate.sh — `make lint` and
# `make cppcheck` both exit 0 when the analyser is absent.
check "cppcheck static analysis" "cppcheck cmake ninja" 'make cppcheck'
check "clang-tidy" "clang-tidy cmake ninja" \
  'make lint 2>&1 | tee /dev/stderr | grep -qv "clang-tidy not installed"'

check "clang-format clean" "clang-format" \
  'find src -name "*.cc" -o -name "*.cpp" -o -name "*.h" 2>/dev/null \
     | xargs -r clang-format --dry-run --Werror'

[ "$TIER" = "quick" ] && { report "$TIER"; exit $?; }

# --- full ------------------------------------------------------------------

check "builds clean" "cmake ninja" 'make build'

# Never a bare ctest — see the note in Arches-Server/.claude/validate.sh.
# `make test` applies -L arches_client.
check "ctest (label=arches_client)" "cmake ninja ctest" 'make test'

report "$TIER"
