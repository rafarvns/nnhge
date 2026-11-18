# Tasks 005: Text — the built-in NNHGE font

Status: in progress (approved 2026-11-18)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task.

## Stage 1 — pure text logic

- [x] **T1.1** UTF-8 decoder — files: `core/src/text/utf8.{hpp,cpp}` — test: `tests/text/test_utf8.cpp` — covers FR-2
- [x] **T1.2** [P] glyph mapping — files: `core/src/text/glyphs.hpp` — test: `tests/text/test_glyphs.cpp` — covers FR-2
- [x] **T1.3** layout: measure and glyph positions, lines, alignment, scale — files: `core/src/text/layout.{hpp,cpp}` — test: `tests/text/test_layout.cpp` — covers FR-3, FR-4

## Stage 2 — the font

- [ ] **T2.1** `scripts/make_font.py`: ASCII glyphs, replacement glyph, Latin-1 composed with diacritics; writes `builtin_font.cpp` and a preview PNG — verified: preview reviewed by the maintainer
- [ ] **T2.2** generated `core/src/text/builtin_font.{hpp,cpp}` — test: `tests/text/test_builtin_font.cpp` (every glyph but space has pixels, accents differ from their base letter) — covers FR-1, FR-2

## Stage 3 — font texture

- [ ] **T3.1** contract `texture_create_rgba`; fake and pc implementations — test: conformance `texture_create_rgba_accepts_pixels` — covers FR-7
- [ ] **T3.2** atlas built at `gfx_init`, reserved engine slot, tag `font`, not in game stats — files: `text/font_texture.cpp`, `gfx/gfx_frame.cpp` — test: `tests/text/test_font_texture.cpp` — covers FR-1, FR-8
- [ ] **T3.3** 3DS `texture_create_rgba` (tiling, byte order) — files: `texture_3ds.cpp` — verified on hardware in T6.3 — covers FR-7

## Stage 4 — drawing text

- [ ] **T4.1** `TextOptions`, `draw_text`, `measure_text` → draw list (alignment, scale, layer, camera, shadow, capacity) — files: `nnh/gfx.hpp`, `gfx/draw_text.cpp` — test: `tests/text/test_draw_text.cpp` — covers FR-3, FR-4, FR-8

## Stage 5 — debug text on the font

- [ ] **T5.1** debug pass: debug text and overlay rectangles laid out into glyph/shape items, own 4096-item buffer, after the 2D passes; `line_height()` = 10 — files: `debug/debug_pass.cpp`, `debug/debug.cpp`, `debug/overlay.cpp` — test: `tests/debug/test_debug_pass.cpp`, overlay tests via the internal hook — covers FR-6
- [ ] **T5.2** remove `debug_text`, `debug_line_height`, `fill_rect` from the contract and the backends (3DS text buffer included) — covers FR-6, FR-7

## Stage 6 — example, docs, closing

- [ ] **T6.1** `examples/text`: character set, Portuguese sentence, alignments, scales, shadow, a measured box; screenshots — covers AC-2
- [ ] **T6.2** `examples/sprites` HUD box sized with `measure_text` — covers AC-3
- [ ] **T6.3** run `examples/text` and `examples/sprites` on the 3DS, compare with the screenshots — maintainer — covers AC-3, FR-5
- [ ] **T6.4** docs: new `docs/api/text.md`; `gfx.md`, `debug.md`, `rendering.md`, `platforms.md` (contract), `sdl3.md`, `devkitpro-3ds.md` (tiling), `simulator.md`; CLAUDE.md — covers AC-5
- [ ] **T6.5** budgets, Release strings check, Linux gcc + clang ASan/UBSan, screenshot references — covers AC-4, AC-5
- [ ] **T6.6** `docs/ROADMAP.md` checkbox; spec, plan and tasks status → implemented
