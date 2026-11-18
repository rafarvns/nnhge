# Tasks 009: Packaging — icon, metadata and installable `.cia`

Status: in progress (approved 2026-11-18)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops (script tests first). `[P]` = can be done in parallel with the previous task.

## Stage 1 — checks and default art

- [x] **T1.1** `nnh_check_icon.cmake` — test: ctest `icon_check_*` + fixtures `tests/data/package/` — covers FR-2
- [x] **T1.2** [P] `nnh_check_package.cmake` (ID, product code, version, lengths in characters) — test: ctest `package_check_*` — covers FR-5
- [x] **T1.3** [P] `nnh_check_banner.cmake` (256×128 PNG, WAV ≤ 3 s, PCM 16-bit) — test: ctest `banner_check_*` — covers FR-5
- [x] **T1.4** `scripts/make_package_art.py` → `cmake/3ds/default_icon.png`, `default_banner.png`, `default_banner.wav` — covers FR-2, FR-3

## Stage 2 — metadata in nnh_add_game

- [ ] **T2.1** `VERSION`, default icon + icon check on 3DS, `NNH_GAME_TITLE` → `AppConfig::title` default — files: `nnh_game.cmake`, `<nnh/app.hpp>` — test: examples build; `hello` screenshot title; unit test of the default — covers FR-1, FR-2
- [ ] **T2.2** `CIA` option group parsed and checked at configure (no tools yet) — test: ctest configures `tests/data/package/game/` (a tiny game) with good and bad options — covers FR-1, FR-5

## Stage 3 — tools and the `.cia`

- [ ] **T3.1** `nnh_ctr_tools.cmake`: pinned downloads, `NNH_MAKEROM`/`NNH_BANNERTOOL` overrides, no download without `CIA` — test: ctest on the tiny game (no `_tools/` without CIA) — covers FR-6
- [ ] **T3.2** `cmake/3ds/nnhge.rsf` + `nnh_cia.cmake`: banner, makerom, `<game>_cia`, `<game>_dist`, `dist` — test: `examples/audio` builds a `.cia` on Windows (3ds-release, 3ds-dev) and on VM 120 — covers FR-3, FR-7
- [ ] **T3.3** `examples/audio`: version, icon, banner, jingle, `CIA UNIQUE_ID 0xE0A01` — covers AC-2

## Stage 4 — verification and docs

- [ ] **T4.1** docs: new `docs/guides/packaging.md`; `cmake.md` (`nnh_add_game` reference), `devkitpro-3ds.md` (`.cia`), `REFERENCES.md`, decision D22, CLAUDE.md, getting-started — covers AC-5
- [ ] **T4.2** budgets without `CIA` (configure + build time, `hello.3dsx` size) — covers AC-4
- [ ] **T4.3** maintainer: `hello` icon in the Homebrew Launcher; `audio.cia` with FBI (banner, jingle, play, exit, uninstall) — covers AC-1, AC-2
- [ ] **T4.4** ROADMAP M5; spec, plan and tasks status → implemented
