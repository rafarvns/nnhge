# Tasks 011: NNH Link — build and run from the console

Status: in progress (stages 1–2 done 2026-11-18)
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

- [ ] **T3.1** server id, pairing store (`nnh-pairs.ini`), code display, discovery answer, real builds with streamed output, log listener — covers FR-1, FR-3

## Stage 4 — the engine's 3DS extensions

- [ ] **T4.1** `<nnh/3ds/network.hpp>` (soc, shared with the log redirect) — covers FR-2
- [ ] **T4.2** `<nnh/3ds/launch.hpp>` (`hb:ldr`) and return-to-Link at shutdown — test: argv building — covers FR-4

## Stage 5 — the NNH Link app

- [ ] **T5.1** `link_state` + storage — tests with a fake connection — covers FR-2, FR-3, FR-5
- [ ] **T5.2** scenes (servers, pairing pad, games, build) — simulator screenshots — covers FR-2, FR-6
- [ ] **T5.3** platform files (3DS launch, PC none); 3DS build — covers FR-4, FR-6

## Stage 6 — verification and docs

- [ ] **T6.1** docs: new `docs/guides/nnh-link.md`; `nnh-tool.md` (serve), `security.md`, api pages of the extension headers, decision D24, CLAUDE.md, project-structure (`link/`, `apps/`) — covers AC-4
- [ ] **T6.2** Linux + ASan/UBSan, fuzzing on VM 120 — covers AC-3
- [ ] **T6.3** maintainer: the whole loop from the console on the Old 3DS XL (AC-1)
- [ ] **T6.4** ROADMAP M6; spec, plan and tasks status → implemented
