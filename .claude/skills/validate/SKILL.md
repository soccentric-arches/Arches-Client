---
name: validate
description: Running and interpreting Arches-Client's validation/verification gate (.claude/validate.sh quick|full) and the underlying make lint/cppcheck/format targets. Use when asked to validate, verify, check repo health, or before closing out a change.
---

# Validate (verification gate)

The single entry point for "is this repo healthy" is `.claude/validate.sh`,
sourcing the shared `.claude/lib/validate-lib.sh`. It is **human-owned** — an
agent may run it, never edit it (fix the underlying issue instead, or raise it
as a Lessons Learned note).

```
./.claude/validate.sh quick   # seconds
./.claude/validate.sh full    # + build + ctest, minutes
```

Also reachable as the `/validate` command (`.claude/commands/validate.md`,
`argument-hint: [quick|full]`), and runs automatically on the `Stop` hook
(`.claude/settings.json`) at the `quick` tier.

## Exit code contract (fleet-wide, all repos use this)

- `0` — every check ran and passed → **PASS**.
- `1`–`76` — a check ran and failed → **FAIL**. Report the failing check and its output; don't fix anything unless asked.
- `77` — one or more checks never ran (missing toolchain, missing hardware) → **BLOCKED**, *not* a pass. Say which checks were skipped and what installing/hardware would unblock them.

Never report a `77` as a success, and never substitute your own judgement for a check that didn't run. Set `ARCHES_VALIDATE_ALLOW_PARTIAL=1` only if you deliberately want an all-BLOCKED-or-PASS run to exit `0` instead of `77` (off by default).

## What `quick` checks

- Sibling `../Arches-HAL/protos` checkout is present (build would fail without it).
- No `protobuf_generate|grpc_generate|protoc` invocations anywhere in this repo (protos must only ever come from `hal_protos_shared`).
- Some adapter/registry dispatch mechanism exists under `src/` (a HAL module that builds but was never registered is invisible to the compiler).
- `cmake -GNinja -DBUILD_TESTS=ON -S . -B build/native` configures cleanly.
- `make cppcheck` — best-effort; reports "not installed" rather than failing if cppcheck is absent (still `77`-worthy if genuinely blocked, see caveat below).
- `make lint` (clang-tidy) — same best-effort shape.
- `clang-format --dry-run --Werror` over all `src/**/*.cc|*.cpp|*.h`.

## What `full` adds

- `make build` — full native build must succeed.
- `make test` — `ctest -L arches_client` (never bare `ctest`; see the `native-test` skill).

## Caveat: `make lint`/`make cppcheck` exit 0 when the tool is absent

The raw Makefile targets are intentionally lenient (so a missing toolchain
doesn't fail `make lint` for a contributor who just wants to build). The
`validate.sh` wrapper is what turns "tool not installed" into a `77 BLOCKED`
rather than a silent pass — that's the reason to always validate through
`.claude/validate.sh`, not by calling `make lint`/`make cppcheck` directly,
when the result needs to be trustworthy.
