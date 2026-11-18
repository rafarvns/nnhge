# Tasks 010: The `nnh` tool and `nnh run`

Status: draft
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task.

## Stage 1 — the tool and its pure pieces

- [ ] **T1.1** `tools/nnh` target in the PC presets + `nnh_tool_tests` with the in-house framework; `nnh help` — covers FR-1
- [ ] **T1.2** `zlib_stored` (Adler-32, stored blocks) — test: `test_zlib_stored.cpp` — covers FR-3
- [ ] **T1.3** [P] `netloader` framing (header, chunks ≤ 16 384, args) — test: `test_netloader.cpp` — covers FR-3
- [ ] **T1.4** [P] `address` (parse, private/loopback) and `fbi` message — tests: `test_address.cpp`, `test_fbi.cpp` — covers FR-4, FR-6, FR-7
- [ ] **T1.5** [P] `log_view` (parse, colour, pass-through) + mini-fuzz — test: `test_log_view.cpp` — covers FR-5
- [ ] **T1.6** [P] `manifest`, `user_config`, `cli` — tests — covers FR-2, FR-4

## Stage 2 — network

- [ ] **T2.1** `net` sockets (Winsock/BSD), timeouts, broadcast, local address — test: loopback echo — covers FR-3, FR-4
- [ ] **T2.2** netloader client session + log listener — test: loopback fake netloader and fake game — covers FR-3, FR-5
- [ ] **T2.3** HTTP file server + FBI exchange — test: loopback fake FBI — covers FR-6, FR-7

## Stage 3 — commands

- [ ] **T3.1** games manifest written by `nnh_add_game()`; `repo` (root, presets, CMake through devkitPro on Windows) — covers FR-2
- [ ] **T3.2** `nnh build`, `nnh run` (3DS and `--pc`), saved logs, remembered console — covers FR-2, FR-3, FR-4, FR-5
- [ ] **T3.3** `nnh install` — covers FR-6

## Stage 4 — verification and docs

- [ ] **T4.1** docs: new `docs/guides/nnh-tool.md`; getting-started, debugging-on-3ds, packaging (`nnh install` replaces `scripts/fbi_install.py`), CLAUDE.md, decision D23, `tools/` in project-structure — covers AC-5
- [ ] **T4.2** Linux gcc + ASan/UBSan on VM 120 (tool tests included); Windows — covers AC-4
- [ ] **T4.3** maintainer: `nnh run hello`, `nnh run audio`, `nnh install audio` on the Old 3DS XL — covers AC-1, AC-2
- [ ] **T4.4** ROADMAP M6 first item; spec, plan and tasks status → implemented
