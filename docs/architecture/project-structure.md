# Project structure

Code is easy to find when every concept has one obvious place. These rules apply to the engine, tools,
examples and tests.

## Folder map

```
engine/
  core/include/nnh/<module>.hpp              public API, one header per module (what games include)
  core/src/<module>/<concept>.cpp|.hpp       core implementation, internal headers (a pure file there may be
                                             shared with a host tool, e.g. remote/remote_wire.cpp with tools/nnh,
                                             tilemap/map_format.hpp, map_layout, map_validate*, the pure data/
                                             files — schema, fingerprint, validators — and animation/
                                             sprite_format.hpp, sprite_layout, sprite_sequence, sprite_validate
                                             with tools/assetc)
                                             font/font_format, font_validate, page_codec and loc/loc_file,
                                             loc_validate, plural (spec 029) with tools/assetc too
  core/src/core/                             engine-wide internal state shared by modules; pure helpers with no
                                             engine includes that host tools compile too (crc32)
  core/contract/nnh/platform/<area>.hpp      platform contract (what each platform library implements)
  platforms/<system>/CMakeLists.txt          nnh::platform_<system> (pc, 3ds, fake)
  platforms/<system>/src/<area>_<system>.cpp one file per contract area per system
  platforms/<system>/include/nnh/<system>/   extension headers (optional, system-only features)
  platforms/<system>/README.md               requirements, limits, capabilities
  platforms/shared/                          code several platforms compile: stdio_fs.cpp, raster3d/ (the CPU
                                             rasteriser of the PC's and the fake's 3D, spec 026 D41; its lighting
                                             tables are compiled by the 3DS backend too)
  platforms/3ds/shaders/                     PICA200 vertex shaders (picasso), embedded by the build (spec 026)
tests/
  framework/                                 the tiny test framework; alloc_counter.* (counting operator new,
                                             linked into nnh_tests only)
  <module>/test_<concept>.cpp                mirrors engine/core/src/<module>/
  platform_conformance/                      contract tests, linked against every platform
  data/<kind>/                               test inputs (images, sounds, asset lists, package; maps: Tiled
                                             fixtures good.tmj and bad_*.tmj for the converter's ctests;
                                             data_assets: bad/<rule>.enemy with its expected message, index/
                                             cases, the test games game/ and broken_game/;
                                             aseprite: make_fixtures.py, good*.ase.json, bad_*.ase.json)
  data_assets/                               the data module's tests (spec 014; tests/data/ holds fixtures)
  <example>/test_<concept>.cpp               an example's pure logic, compiled from examples/<example>/
                                             (tests/platformer: the benchmark's gate, camera, coins, player,
                                             replay; replay_matches_script.cmake checks tests/level.nnhs;
                                             tests/animation_example: examples/animation's self-check;
                                             tests/loc_example: examples/localization's self-check over its
                                             converted assets)
  font/ loc/ text/                           spec 029: fonts (font_fixture.hpp builds .nnhfont in memory),
                                             localization (loc_fixture.hpp writes tables and manifests), the shared
                                             wrapper and run layout; data/fonts: make_test_fonts.py's fixtures and
                                             golden.cmake, routing.cmake; data/strings_decl: LANGUAGES cases
  consumer/                                  spec 032: a game project kept with the tests (nnh.toml, the six presets,
                                             game/ with every nnh_add_game keyword, small/, plain/), built against
                                             the engine by the consumer ctests (tests/consumer.cmake)
  consumer_tools/                            their driver (run_consumer.cmake, consumer_engine.cmake,
                                             compare_flags.cmake), plain_only/, unknown_platform/, copy_project.sh
examples/<name>/                             one small program per feature (main.cpp + CMakeLists.txt)
  tests/*.nnhs                               test scripts (spec 013): nnh run <name> [--pc] --script ...
                                             (guide: guides/testing-on-the-console.md)
docs/
  api/<module>.md                            reference for each public module
  architecture/  process/  tech/  guides/
specs/NNN-<slug>/                            spec.md, plan.md, tasks.md per feature
link/                                        nnh_link (spec 011, D24): sockets, SHA-256/HMAC, messages, frames,
  include/nnh_link/  src/  tests/(fuzz/)     discovery, pairing, server/client sessions; built for PC and 3DS,
                                             shared by nnh and the app
apps/<app>/                                  apps made with NNHGE that ship with it, in every preset: apps/nnh-link
  src/  tests/                               (spec 011): pure logic (link_state, storage, pad, layout) tested
                                             against fakes; views and one scene; platform_link_<system>.cpp
tools/<tool>/                                host tools: tools/nnh (the nnh command, spec 010; src/remote/: test
                                             scripts and the console remote's PC side, spec 013); tools/assetc
                                             (nnh-assetc, build-time conversions, spec 012, D27: Tiled maps; src/
                                             json*, tiled_*, map_writer; spec 014: toml*, schema_read, data_*,
                                             data_index_build, index_writer; spec 029: font_desc, charsets*, ttf_check,
                                             ttf_raster, bmfont_*, png_pages, glyph_pack, font_writer, convert_font,
                                             strings_*, plural_*; tests/ with the fuzz targets; built by
                                             cmake/nnh_host_tools.cmake, standalone-capable CMakeLists.txt);
                                             tools/data_schema (spec 014: main() of each game's data schema
                                             program, cmake/nnh_data.cmake)
third_party/<library>/                       vendored code, unmodified and pinned (README: commit, SHA-256,
                                             license); exempt from the file-size rules
cmake/                                       CMake helpers (nnh_game.cmake and what it includes; spec 032:
                                             nnh_platform, nnh_version, nnh_game_defaults, nnh_optional_parts)
engine/core/archive/                         spec 032: the static nnh_core archived from nnh_core_objects (a
                                             response file in 3DS presets, FR-19)
scripts/                                     small helpers (dkp.sh, docs tool, hooks)
```

A game made with NNHGE follows the same idea, in a game project of its own ([guide](../guides/game-projects.md)):

```
mygame/
  CMakeLists.txt  CMakePresets.json  nnh.toml   the project: the engine pinned, the six presets, nnh's marker
  src/main.cpp                       init, push first scene, run
  src/scenes/<name>_scene.hpp|.cpp   one scene per pair
  src/<feature>/...                  game modules (player/, enemies/, ui/)
  src/events.hpp                     the game's event structs
  assets/
```

## File rules

| Rule | Limit |
|------|-------|
| One concept per file (one class, or one cohesive group of free functions) | always |
| `.cpp` length | aim ≤ 200 lines, **hard limit 400** |
| Header length | aim ≤ 150 lines |
| Function length | aim ≤ 40 lines |
| Public headers per module | exactly one: `<nnh/<module>.hpp>` |
| Internal headers | under `engine/core/src/`, never included by games or public headers |
| Source lists in CMake | explicit (no `file(GLOB)`) |

When a file grows past the aim, split it by concept before adding more (e.g. `video_pc.cpp` and
`input_pc.cpp` instead of one `platform_pc.cpp`). A pull request that crosses a hard limit must split.

## Naming

- Files and folders: `snake_case`. Backend files end with the target: `input_3ds.cpp`, `input_pc.cpp`.
- Types `PascalCase`, functions and variables `snake_case`, constants `kPascalCase`, macros `NNH_UPPER`.
- Members end with `_` in classes with invariants (`score_`); plain structs use bare names.
- Tests: `test_<concept>.cpp`; test cases `behaviour_when_condition`.

## Where does new code go?

| You are adding... | Put it in |
|-------------------|-----------|
| A new engine feature (audio, sprites) | New module: see the `nnh-new-module` skill |
| Platform-specific code | `engine/platforms/<system>/src/<area>_<system>.cpp`, behind a contract header `nnh/platform/<area>.hpp` |
| Logic shared by modules | The module that owns the concept; expose an internal header in its folder |
| A helper used once | Next to its user, in an anonymous namespace |
| A test | `tests/<module>/test_<concept>.cpp` |
| A demo of a feature | `examples/<feature>/` |

Live values (spec 030): `engine/core/src/live/` (the module: names, steps, value text, the registry, the panel, the
remote side), Development-only core files `app/dev_pause.*`, `debug/dev_controls.*`, `debug/overlay_pages.*`,
`debug/debug_held.cpp`, `gfx/gfx_replay.*`, `audio/audio_hold.cpp`, `remote/live_wire.*`, `remote/remote_live.cpp`;
`third_party/ryu/` (Ryu's `f2s`).

Hot reload (spec 031, Debug/Development only): `engine/core/src/reload/` (the wire, the receiver, names and the overlay —
pure, the wire and names also in `nnh` —, received files, reads through the overlay, the batch engine, the asset-index
kind, the apply point, `reload_hooks.*`: the opt-in hooks set when a kind registers), `remote/remote_reload.*` (the
session's glue), one `<kind>_reload.cpp` per reloaded kind in its own module (`gfx/texture_reload.*`,
`data/data_reload.cpp`, `tilemap/map_reload.cpp`, `loc/loc_reload.cpp`, `audio/sound_reload.cpp`,
`font/font_reload.cpp`, `mesh/mesh_reload.cpp`), the contract `nnh/platform/reload.hpp` with `reload_<system>.cpp` per
platform; `tools/nnh/src/reload/` (the output mirror, the source watcher, the asset build, the batch encoder, the
controller, the paths file, `asset_swap.*`: a script's swaps with their journal) and `tools/nnh/src/remote/script_reload.*`,
`script_runner_reload.cpp`, `script_runner_swap.cpp`, `commands/run_reload.cpp`.

> Status: design (spec 033, D56 proposed). `nnh new` and `nnh doctor` add `tools/nnh/template/` (the game-project
> template as real files, embedded into `nnh` by `tools/nnh/embed_template.cmake` and compiled by the engine's tests as
> `nnh_template_check`), `tools/nnh/src/new/` (name rules, engine location, rendering, writing) and
> `tools/nnh/src/doctor/` (probes, version parsers, one check group per file, the report); see
> [plan 033](../../specs/033-nnh-new-and-doctor/plan.md).
