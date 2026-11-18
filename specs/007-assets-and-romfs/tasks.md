# Tasks 007: Assets — build pipeline and romfs

Status: approved (2026-11-18)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task.

## Stage 1 — pure core logic

- [ ] **T1.1** asset path rules — files: `core/src/assets/asset_path.{hpp,cpp}` — test: `tests/assets/test_asset_path.cpp` — covers FR-6
- [ ] **T1.2** [P] the index: parse, case-sensitive lookup, "did you mean" hint, size bound — files: `core/src/assets/asset_index.{hpp,cpp}` — test: `tests/assets/test_asset_index.cpp` — covers FR-6

## Stage 2 — contract, fake, API

- [ ] **T2.1** contract `assets_mount/unmount`, `asset_size`, `asset_read`; fake table; pc/3ds stubs — test: conformance `missing_assets_mount_false_or_empty` — covers FR-8
- [ ] **T2.2** `<nnh/assets.hpp>`: index at init, exists/size/read/load, `AssetData`, errors, tag `assets` — files: `core/src/assets/assets.cpp`, `app.cpp` — test: `tests/assets/test_assets.cpp` — covers FR-5, FR-7
- [ ] **T2.3** `load_texture` from assets first, then `IMAGES` — files: `gfx/textures.cpp` — test: `tests/assets/test_asset_textures.cpp` — covers FR-4

## Stage 3 — build pipeline

- [ ] **T3.1** `nnh_assets.cmake` + `ASSETS` in `nnh_add_game`: per-file convert/copy into `<bin>/assets/`, `<game>_assets` target — covers FR-1, FR-2, FR-3
- [ ] **T3.2** `nnh_asset_index.cmake`: sorted index, invalid names and case clashes fail the build, stale outputs removed — test: ctest script cases with fixture folders — covers FR-1, FR-6
- [ ] **T3.3** 3DS: romfs asset target passed to `ctr_create_3dsx` — covers FR-3

## Stage 4 — platforms

- [ ] **T4.1** PC: `<exe dir>/assets/` reader — files: `assets_pc.cpp` — covers FR-3
- [ ] **T4.2** 3DS: `romfsInit`/`romfs:/` reader — files: `assets_3ds.cpp` — covers FR-3

## Stage 5 — example, docs, closing

- [ ] **T5.1** `examples/sprites` on `ASSETS` + `levels/level1.txt`; screenshots identical — covers AC-2
- [ ] **T5.2** docs: new `docs/api/assets.md`; `texture.md`, `cmake.md` (ASSETS, GLOB exception), `devkitpro-3ds.md` (romfs), `sdl3.md`, decisions D6/D19, CLAUDE.md — covers AC-5
- [ ] **T5.3** budgets (edit one asset), Release strings, Linux gcc + clang ASan/UBSan — covers AC-4
- [ ] **T5.4** run `examples/sprites` on the 3DS from the romfs — maintainer — covers AC-3
- [ ] **T5.5** `docs/ROADMAP.md` M3 checkboxes; spec, plan and tasks status → implemented
