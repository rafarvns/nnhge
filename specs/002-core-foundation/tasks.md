# Tasks 002: Core foundation

Status: in progress
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each stage ends green: PC + 3DS build, tests, CI, docs updated (nnh-docs). `[P]` = parallelizable.

## Stage 1 — Core / platform split (FR-P1–P3)
- [x] **T1.1** Move code to `engine/core/{include,src}` and `engine/platforms/{pc,3ds}/src`; split `platform.hpp` into `engine/core/contract/nnh/platform/{app,video,input}.hpp` — test: existing unit tests + hello screenshot unchanged
- [x] **T1.2** CMake targets `nnh::core_contract`, `nnh::core`, `nnh::platform_pc`, `nnh::platform_3ds`, `nnh::engine` (link multiplicity) — test: all presets build from clean
- [x] **T1.3** `nnh::platform_fake` (scriptable buttons, recorded draw calls, manual clock) — test: `tests/platform_conformance/` skeleton passes on fake — covers FR-P3
- [x] **T1.4** Conformance tests also run against `platform_pc` headless in CI — covers FR-P3
- [x] **T1.5** Platform READMEs; update project-structure, platforms ("Status: implemented" for layout), CLAUDE.md, skills paths — measure build times

Stage 1 notes (2026-11-17): link multiplicity was not needed yet (platforms don't call core); add it in
stage 4 when platform sinks log through core. The fake's manual clock comes with the time contract
(T3.1). New: CI "layering" check (no SDK include in `engine/core`), 11 unit + 5×2 conformance tests,
mutation check passed. Times: edit game `.cpp` → PC 1.3 s, `.3dsx` 2.1 s; clean 3DS build 3.6 s.
## Stage 2 — Build flavors (FR-B1–B2)
- [x] **T2.1** Presets `*-dev`, flavor defines on `nnh::core` — test: `tests/flavor/` compile-time check of the active flavor
- [x] **T2.2** docs: cmake.md, getting-started, CLAUDE.md commands
## Stage 3 — Time (FR-T1–T2)
- [ ] **T3.1** contract `time.hpp` + pc/3ds/fake — test: conformance (monotonic)
- [ ] **T3.2** `<nnh/time.hpp>` Duration, now, delta — test: `tests/time/test_clock.cpp`
- [ ] **T3.3** FixedStep — test: `tests/time/test_fixed_step.cpp` (steps per second, catch-up cap)
- [ ] **T3.4** `docs/api/time.md`
## Stage 4 — Logging (FR-L1–L6)
- [ ] **T4.1** format + truncation — test: `tests/log/test_format.cpp`
- [ ] **T4.2** levels, channels, compile-out macros — test: `tests/log/test_filter.cpp`
- [ ] **T4.3** ring buffer — test: `tests/log/test_ring.cpp`
- [ ] **T4.4** sink registry — test: `tests/log/test_sinks.cpp`
- [ ] **T4.5** [P] PC console sink (colors) · 3DS stdout/3dslink + debugger sinks — test: example output on PC; 3dslink terminal on console
- [ ] **T4.6** replace ad-hoc `SDL_Log` calls with channels; `docs/api/log.md`
## Stage 5 — Errors (FR-E1–E4)
- [ ] **T5.1** `Result<T>` / `Error` — test: `tests/result/test_result.cpp`
- [ ] **T5.2** assert macros + fail path (fake `crash_show` records) — test: `tests/assert/test_assert.cpp`
- [ ] **T5.3** crash report builder (ring snapshot, location, registers) — test: `tests/crash/test_report.cpp`
- [ ] **T5.4** PC `crash_show` + signal/unhandled-exception hooks — test: example screenshot of crash screen
- [ ] **T5.5** 3DS `crash_show` (text console) + `threadOnException` handler — test: on console (assert and null write)
- [ ] **T5.6** `docs/api/assert.md`, `docs/api/result.md`
## Stage 6 — Memory (FR-M1–M3)
- [ ] **T6.1** contract `memory.hpp` + pc (emulated linear/VRAM budgets) / 3ds (linearAlloc, vramAlloc) / fake
- [ ] **T6.2** tagged allocator bookkeeping, stats, budgets — test: `tests/memory/test_accounting.cpp`
- [ ] **T6.3** leak report at shutdown — test: `tests/memory/test_leaks.cpp`
- [ ] **T6.4** move engine allocations (citro2d text buffer etc.) to tags where possible; `docs/api/memory.md`
## Stage 7 — File safety (FR-S1–S3)
- [ ] **T7.1** path validation — test: `tests/fs/test_path.cpp` (accept/reject tables)
- [ ] **T7.2** contract `fs.hpp` + pc/3ds/fake (fake simulates interrupted writes) — test: conformance
- [ ] **T7.3** `nnh::fs` read/write/atomic write — test: `tests/fs/test_atomic.cpp`
- [ ] **T7.4** save format (header, CRC-32, backup fallback) — test: `tests/fs/test_save.cpp`
- [ ] **T7.5** file log sink + `crash.txt` writing — test: fake fs contents
- [ ] **T7.6** fuzz targets `fuzz_path`, `fuzz_save`, seed corpus, corpus runner in `nnh_tests`; CI job `fuzz` (clang + libFuzzer + ASan)
- [ ] **T7.7** `docs/api/fs.md`
## Stage 8 — System, overlay, profiler (FR-P4–P5, FR-D1–D3)
- [ ] **T8.1** contract `system.hpp`; `<nnh/system.hpp>` capabilities/info — test: conformance + fake
- [ ] **T8.2** `<nnh/3ds/console.hpp>` extension with `#error` guard — test: building a file that includes it for PC fails (CI compile-fail check)
- [ ] **T8.3** profiler scopes table — test: `tests/profile/test_scopes.cpp` with fake clock
- [ ] **T8.4** overlay (fps, frame graph, memory) + `fill_rect` in contract — test: screenshot
- [ ] **T8.5** simulator controls (F1, Pause, F10, F11, F12)
- [ ] **T8.6** `docs/api/system.md`, `docs/api/profile.md`, update `docs/api/debug.md`
## Stage 9 — Closing (FR-S4, FR-D4, AC-3–AC-7)
- [ ] **T9.1** CI: `permissions` per workflow, actions pinned by SHA
- [ ] **T9.2** `examples/foundation/` (logs, overlay, profile, save/load, deliberate assert combo)
- [ ] **T9.3** CI check: Release `.elf` has no trace/assert strings (AC-5); size budget measured
- [ ] **T9.4** `docs/guides/debugging-on-3ds.md` (GDB via Luma3DS, UNITINFO, Luma dumps)
- [ ] **T9.5** run on 3DS: logs over 3dslink, crash screen, `crash.txt`, conformance example
- [ ] **T9.6** build-time baseline update in ROADMAP; architecture pages "Status: implemented"; ROADMAP checkboxes; spec/plan/tasks → implemented
