# The `nnh new` template

The files `nnh new <name>` writes (spec [033](../../../specs/033-nnh-new-and-doctor/spec.md), decision D56). They are
embedded into `nnh` at build time by `tools/nnh/embed_template.cmake`, in the order of `NNH_TEMPLATE_FILES` in
`tools/nnh/CMakeLists.txt` (an explicit list: this README is not embedded). Text files are embedded with LF line
endings.

- `*.in` files are rendered: `@name@`, `@title@`, `@author_line@`, `@unique_id@`, `@engine_url@`, `@engine_sha256@`,
  `@engine_version@`, `@engine_min@`, `@docs_base@`, `@engine_state@` (`tools/nnh/src/new/template_render.cpp`). An
  unknown placeholder fails `nnh_tool_tests`.
- `gitignore` and `gitattributes` are written as `.gitignore` and `.gitattributes` (stored without the dot so they never
  act on the engine repository).
- The C++ sources and `tests/smoke.nnhs` have no placeholders (the title comes from `NNH_GAME_TITLE`, which
  `nnh_add_game()` defines), so the engine's own PC build compiles them as the check game `nnh_template_check` with the
  engine's warnings as errors, and the ctest `nnh_template_smoke` runs the smoke script against it.
- `CMakePresets.json` is byte-equal to `tests/consumer/CMakePresets.json` (a unit test compares them).

## Provenance of the assets (zlib, like the engine)

| File | Made by |
|---|---|
| `assets/art/player.png` | drawn by `tools/nnh/make_template_player.py` (a 32x32 RGBA block with a face; deterministic) |
| `assets/sfx/blip.wav` | a copy of `tests/data/sounds/ok_mono16.wav`: a 0.1 s sine sweep (400 to 800 Hz, 16 kHz, 16-bit mono) synthesized by `tests/data/sounds/make_sounds.py` |

Both were made for the engine; neither comes from a third party.
