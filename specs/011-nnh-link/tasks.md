# Tasks 011: NNH Link — build and run from the console

Status: in progress (stages 1–2 done 2026-11-18, stage 3 2026-11-19, stages 4–5 2026-11-20)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task.

## Stage 1 — the link library: crypto and messages

- [x] **T1.1** `link/` library `nnh_link` (PC and 3DS) + `nnh_link_tests`; sockets moved from `tools/nnh` — covers FR-7
- [x] **T1.2** SHA-256 and HMAC-SHA-256 — test: FIPS 180-4 and RFC 4231 vectors — covers FR-3
- [x] **T1.3** [P] message codec (framing, types, sequence + MAC) and discovery datagrams — tests + fuzz targets — covers FR-3, FR-7
- [x] **T1.4** pairing and authentication state machines (both sides) — tests: wrong code, rate limit, impostor, unpaired, replay, tampering — covers FR-3

## Stage 2 — sessions

- [x] **T2.1** server session (list, build via an injected builder, file, launch) and client session — tests: loopback, fake builder — covers FR-1, FR-5 (`link/src/{connection,server_session,client_session}.cpp`, `link/tests/test_sessions.cpp`)
- [x] **T2.2** failures: server gone mid-file, build failed, timeouts, cancel — covers FR-5 (`link/tests/test_session_failures.cpp`; also a damaged file, more data than announced, no SD space)

## Stage 3 — `nnh serve`

- [x] **T3.1** server id, pairing store (`nnh-pairs.ini`), code display, discovery answer, real builds with streamed output, log listener — covers FR-1, FR-3 (`tools/nnh/src/serve/`, `commands/serve.cpp`, `nnh unpair`; tests `tools/nnh/tests/test_serve.cpp`; checked by hand against a throwaway client: pairing, re-pairing, real 3DS builds, a failed build). The games manifest gained the title (`name|dir|cia|title`) for the console's list

## Stage 4 — the engine's 3DS extensions

- [x] **T4.1** `<nnh/3ds/network.hpp>` (soc, shared with the log redirect) — covers FR-2 (`src/network_3ds.cpp`, reference-counted; `nnh::shutdown()` releases what the game left)
- [x] **T4.2** `<nnh/3ds/launch.hpp>` (`hb:ldr`) and return-to-Link at shutdown — test: argv building — covers FR-4 (`src/launch_3ds.cpp`; pure rules `src/hbldr_args.cpp` + `tests/platform_3ds/test_hbldr_args.cpp`; headers refuse PC builds, tested; api page `docs/api/3ds-extensions.md`). On the console: with NNH Link (T6.3)

## Stage 5 — the NNH Link app

- [x] **T5.1** `link_state` + storage — tests with a fake connection — covers FR-2, FR-3, FR-5 (`apps/nnh-link/src/{link_state,storage,pad,layout,device}.cpp`, ports in `ports.hpp`; `nnh_link_app_tests`: 50 tests, 18 of 18 manual mutants killed)
- [x] **T5.2** scenes (servers, pairing pad, games, build) — simulator screenshots — covers FR-2, FR-6 (one scene + `view_*.cpp`; `NNH_LINK_DEMO=<screen>` for screenshots of every screen; checked by hand against a real `nnh serve`: discovery, pairing, real 3DS builds received, a second run without a code)
- [x] **T5.3** platform files (3DS launch, PC none); 3DS build — covers FR-4, FR-6 (`platform_link_{3ds,pc}.cpp`; `nnh-link.3dsx` builds in 3ds-dev and 3ds-release — it first exposed a link error in `link/src/net.cpp` (`gethostid` without `extern "C"`), fixed)

## Stage 6 — verification and docs

- [ ] **T6.1** docs: new `docs/guides/nnh-link.md`; `nnh-tool.md` (serve), `security.md`, api pages of the extension headers, decision D24, CLAUDE.md, project-structure (`link/`, `apps/`) — covers AC-4
- [ ] **T6.2** Linux + ASan/UBSan, fuzzing on VM 120 — covers AC-3
- [ ] **T6.3** maintainer: the whole loop from the console on the Old 3DS XL (AC-1)
- [ ] **T6.4** ROADMAP M6; spec, plan and tasks status → implemented
