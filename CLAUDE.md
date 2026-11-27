# CLAUDE.md

This file guides Claude Code (claude.ai/code) when working in this repository.

## What this is

**NNHGE — Native Nimble Homebrew Game Engine.** An open-source (zlib) C++ game engine that produces
**native executables for homebrew-enabled Nintendo consoles**. First target: **Nintendo 3DS**. Later
targets (Switch, Wii U, …) must fit the same platform layer without rewriting games.

Three pillars:

1. **Engine** — C++ library with a small, well-documented, powerful public API, **event-driven**
   (event bus + scenes).
2. **PC simulator** — the same game code builds as a native desktop app (SDL3; Windows first) that mimics
   the console (two screens, buttons, 60 Hz, 3DS limits). A host build, not an emulator.
3. **NNH Link** (spec 011, `apps/nnh-link` + `nnh serve`) — a console app that asks the PC to build,
   receives the result, runs it and streams logs back. Compilation always happens on the PC
   ([guide](docs/guides/nnh-link.md)).

Artifacts: 3DS → `.3dsx` (Homebrew Launcher) and `.cia` (HOME Menu, planned).

Status and next steps: [docs/ROADMAP.md](docs/ROADMAP.md). Prior art: [docs/REFERENCES.md](docs/REFERENCES.md).

## Where the rules live

| Topic | Read |
|-------|------|
| Non-negotiable rules (checked in every plan) | [docs/process/constitution.md](docs/process/constitution.md) |
| Architecture: events, scenes, patterns, folders, decisions | [docs/architecture/](docs/architecture/) |
| How we work: SDD, TDD, SOLID | [docs/process/](docs/process/) |
| Technology: devkitPro/3DS, CMake, C++ subset, SDL3 | [docs/tech/](docs/tech/) |
| Public API reference | [docs/api/](docs/api/) |
| Feature specs | [specs/](specs/) |

## How we work

```
spec (approved by the maintainer) → plan (Constitution Check) → tasks → TDD per task → review → docs + example → ROADMAP
```

- **No feature code without an approved spec** in `specs/NNN-slug/`. Bug fixes start with a failing test.
- **Test first** for all logic in `engine/core/src/<module>/`; backends are verified by examples, simulator
  screenshots and the 3DS.
- **Small steps**: one roadmap item at a time, working on PC *and* 3DS, documented, with an example.
- **Organized files**: one concept per file, `.cpp` aim ≤ 200 / hard 400 lines, one public header per
  module, explicit CMake source lists ([project-structure.md](docs/architecture/project-structure.md)).
- **Query for state, events for changes**; scenes for game modes; no platform `#ifdef` in game code or
  public headers; no exceptions/RTTI/per-frame heap in the engine.
- **Docs with every change**: after any modification, run the `nnh-docs` skill — update every doc the
  change made false and check links. Repository docs are the source of truth; the
  [GitHub wiki](https://github.com/rafarvns/nnhge/wiki) is generated from them by CI on push to `main`
  (never edit it by hand). A Stop hook (`.claude/settings.json` → `scripts/hooks/docs_reminder.py`)
  sends you back if a turn ends with changed files but no doc change, or with broken links. See
  [docs/process/documentation.md](docs/process/documentation.md).

Project skills (`.claude/skills/`): `nnh-architecture`, `nnh-spec`, `nnh-tdd`, `nnh-new-module`,
`nnh-review`, `nnh-docs`, `nnh-devkitpro`, `nnh-cmake`, `nnh-cpp`, `nnh-sdl3`.
Project agents (`.claude/agents/`): `nnh-spec-writer` → `nnh-architect` → `nnh-tdd-developer` →
`nnh-reviewer`; specialists `nnh-3ds-specialist`, `nnh-build-engineer`. Mapping per step:
[docs/process/README.md](docs/process/README.md).

## Repository layout

Portable core + one library per system, plugged in at link time through the platform contract
([platforms.md](docs/architecture/platforms.md)). Foundation services (spec 002, implemented):
`<nnh/time.hpp>`, `<nnh/log.hpp>`, `<nnh/assert.hpp>`, `<nnh/result.hpp>`, `<nnh/memory.hpp>`,
`<nnh/fs.hpp>` (+ `nnh::save`), `<nnh/system.hpp>`, `<nnh/profile.hpp>`, debug overlay — use them instead
of ad-hoc code ([foundation.md](docs/architecture/foundation.md), demo `examples/foundation`). Event bus
and scenes (spec 001, implemented; demo `examples/scenes`): `<nnh/events.hpp>` — `subscribe`/`publish`/`Subscription`, delivered in
`nnh::frame()` ([api/events.md](docs/api/events.md)); `<nnh/scene.hpp>` — `Scene`, `scenes::push/pop/replace`,
`nnh::run()` ([api/scene.md](docs/api/scene.md)). Full input (spec 003, implemented; demo `examples/input`):
`<nnh/input.hpp>` — buttons, `circle_pad()`/`c_stick()`, touch queries and events; PC keyboard, mouse,
gamepad ([api/input.md](docs/api/input.md)). 2D graphics (spec 004, implemented; demo `examples/sprites`):
`<nnh/texture.hpp>` — `load_texture()` from `nnh_add_game(... IMAGES ...)`; `<nnh/gfx.hpp>` — `gfx::draw`,
shapes, layers, `Camera2D`; recorded and submitted per screen so 3D can be added
([architecture/rendering.md](docs/architecture/rendering.md)). Text (spec 005, implemented; demo
`examples/text`): `gfx::draw_text`/`measure_text` with the built-in NNHGE font, identical on every system;
debug text uses it too ([api/text.md](docs/api/text.md)). Assets (spec 007; demo `examples/sprites`):
`nnh_add_game(... ASSETS assets)` packs a folder (romfs on 3DS, a folder next to the PC program);
`<nnh/assets.hpp>` reads files by case-sensitive path, `load_texture("art/hero")` loads images
([api/assets.md](docs/api/assets.md)). Audio (spec 008, implemented): `<nnh/audio.hpp>` — `load_sound("sfx/jump")` (WAV/Ogg from
assets), `audio::play` voices with volume/pan/pitch/priority, groups and master, `SoundFinished`; streamed
`music::play`/`crossfade` with loop points and fades, `MusicFinished`; Ogg via
vendored `third_party/stb_vorbis`; PC output via SDL 3 + a software mixer, 3DS via ndsp; demo
`examples/audio` ([api/audio.md](docs/api/audio.md)). Collision and tilemaps (spec 012, implemented; demo
`examples/platformer`): `<nnh/collision.hpp>` (pure, only `<nnh/math.hpp>`) — box queries, `collision::move`
through a `Grid` the game describes (its own `Cell` array or a callback; solid and one-way cells, no tunnelling),
`contacts`, `cells_touching` ([api/collision.md](docs/api/collision.md)); `<nnh/tilemap.hpp>` — `Tilemap::create`
(one block, fixed capacities), tilesets, layers, `set_tile`/`fill`, objects, typed properties, animated tiles,
`collision_grid()`; `load_tilemap("levels/level1")` loads a map the build converted from a Tiled `.tmj` in `ASSETS`
(`nnh-assetc`, D27; refusals print `file: reason`; fixtures in `tests/data/maps/`); `gfx::draw_tilemap` draws tile
batches (D26, needs `AppConfig::gfx.max_tiles`, 0 by default; Old 3DS: 4 full layers + 300 sprites at 60 Hz)
([api/tilemap.md](docs/api/tilemap.md), guide [levels.md](docs/guides/levels.md)).
Collision layers and queries (spec 019, implemented; D34; demo `examples/queries`): in
`<nnh/collision.hpp>` — `Layers` (32 bits, the game's own enum), `detects(mask, layers)`, `box_view(...)` over the
game's own arrays (structs by member pointer), `QueryFilter`; `raycast`/`raycast_all` against grids (exact walk: no
tunnelling, no corner leaks) and boxes, `cells_crossed`, `line_of_sight`, `box_cast`, `boxes_at`,
`boxes_overlapping`, `cell_at`; no allocation, the same bits everywhere ([api/collision.md](docs/api/collision.md)).
Tilemaps: layer sets per `collision` layer and solid/one-way tile (Tiled `collision_layers`), `collision_grid(mask)`
([api/tilemap.md](docs/api/tilemap.md#collision-layers)). The platformer uses layered coins and turrets.
Pathfinding on grids (spec 025, implemented, console and Linux checks passed; D40;
demo `examples/pathfinding`):
`<nnh/pathfinding.hpp>` — `PathMap::create(path_map_desc(grid))` (one block, 1 byte per cell: blocked or weight 1..255,
agent sizes 1..4), `sync(grid[, region], PathWeights)` from any `collision::Grid` (a tilemap's `collision_grid(mask)`;
weights from a `path_cost` tile property through a per-tile-id table), `set_cell`/`fill`, `version()`, `Diagonals`
(Never/Always/AtLeastOneWalkable/OnlyIfNoObstacles); `PathSearcher` — integer A* with a fixed total order (f, lower h,
most recent push; N E S W NE SE SW NW; strictly lower g), `find` or `begin`/`step(n)` (stale flag), `max_expanded`,
`partial`, `heuristic_scale`, `path`/`path_points`, `reached`/`expanded`/`cost_to`; `smooth_path` (spec 019's walk in
integers), `cells_to_points`; `DistanceField` (multi-goal Dijkstra, double-buffered, `next`/`next_point`);
`PathFollower`. No allocation after creation; golden `1C0B5992` ([api/pathfinding.md](docs/api/pathfinding.md)).
Entities, tags and object pools (spec 020, implemented, console and Linux checks passed; D35; demo `examples/entities`):
`<nnh/pool.hpp>` — `Pool<T, N>` (inline, no allocation) and `Pool<T>::create(n)` (one block), `Handle<T>` (16-bit slot
+ 16-bit generation: stale use detected), lowest free slot, released slots quarantined until `flush()`, `items()`/`each`
in slot order ([api/pool.md](docs/api/pool.md)); `<nnh/entities.hpp>` — `World::create(desc, kinds)` (one block),
`component<T>(cap[, remove hook])` over the game's plain structs in stable-slot arrays, `spawn`/`destroy` (hooks first,
destroys from hooks queued), `add/get/has/remove<T>`, `each<A, B...>` (first kind's slot order, safe destroy/spawn
inside), `storage<T>()` views (box views read bodies in place, `owner(slot)`), tags `nnh::tag("enemy")` with
`find`/`first`/`each_tagged`; both bind to a scene through D30 (flush before update, clear after `on_exit`)
([api/entities.md](docs/api/entities.md)). The platformer's coins, turrets and sparkles are entities, its bullets a pool.
Prefabs (spec 021, implemented, console and Linux checks passed; D36; demo `examples/prefabs`): `<nnh/prefabs.hpp>` —
`PrefabLibrary::create(desc)` (one block), `add(name, {.base, .tags, .place = nnh::place(&Body::box), .on_spawn},
values...)` (FNV-1a names, a colliding name refused; variants flattened at registration, unreplaced values shared),
`find(name)` → `Prefab` handle, `spawn(world, p[, at])` / `spawn_with(world, p[, at], overrides...)` — bit for bit what
`spawn`/`add<T>(e, value)`/`add_tag` make, all or nothing through one entities-internal reservation
(`src/entities/entities_internal.hpp`), `WhenFull::Assert|Miss` — hooks with `SpawnInfo`, `spawn_map_objects(world,
map)` (a Tiled object's `prefab` property, else its type; unknown explicit names a loud level error; only
`prefab_map.cpp` includes the tilemap header), values from data assets by pattern ([api/prefabs.md](docs/api/prefabs.md),
guide [levels.md](docs/guides/levels.md#placing-prefabs-in-tiled)). The platformer's coins, turrets and sparkles are
prefabs (replay lines unchanged); the example's self-check (golden `7BB7901E`) compares prefab spawns with hand-written
ones over 50 000 operations.
Scene tree and transforms (spec 022, implemented, console and Linux checks passed, budgets set; D37; demo `examples/hierarchy`):
`<nnh/transform.hpp>` (only `<nnh/math.hpp>`) — `Transform2D`, `WorldTransform2D` (affine map + drawing values),
`compose`/`to_world`/`transform_point`/`inverse`/`relative`/`bounds`, `sin_cos` (libm-free, exact quarter turns
`kQuarterTurn`…), the same bits everywhere ([api/transform.md](docs/api/transform.md)); `<nnh/hierarchy.hpp>` — the
engine's one component kind `nnh::Node` (`node_component(cap)`), `Hierarchy` view: `attach/detach(…, KeepWorld|KeepLocal)`,
`can_attach`, queries, `each_descendant`, `propagate()` once per frame (update → pass → camera → draw), `world_now`,
destroying a parent destroys its children; prefabs with children (`nnh::child()`, `spawn_under`); `gfx::draw(screen,
tex, world, options)` ([api/hierarchy.md](docs/api/hierarchy.md)). The platformer's turrets have barrel children.
Tweens and timers (spec 015, implemented; demo `examples/tweens`): `<nnh/tween.hpp>` — 19
libm-free easings (same bits everywhere), `nnh::Timer`, `TweenSet` (float/Vec2/Color by pointer or setter, loops,
yoyo, sequences, `after`/`every`, callbacks or your own finish events) on the frame clock (`Frames`/`FrameTime` in
`<nnh/time.hpp>`); a set built with `{*this, ...}` in a scene is updated and cleared by it (D30, scene attachments)
([api/tween.md](docs/api/tween.md)).
Data assets (spec 014, implemented; D29; demo `examples/data`): `<nnh/data.hpp>` — plain
structs + `NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(hp, .min = 1), ...)` in a schema source named in
`nnh_add_game(... DATA_SCHEMA data_types.cpp ASSETS assets)`; hand-written TOML-subset files `assets/enemies/goblin.enemy`
checked by the build (`nnh-assetc data`/`data-index`, `file:line: reason`, fixtures in `tests/data/data_assets/`) and
converted; `data::load("enemies/goblin", e)`, `count<T>`/`name<T>`, `Ref<T>`/`TextureRef` + `data::path(ref)`;
allocation-free once the index `.nnh_data` is loaded. Schema sources compile for the host too: declarations only
([api/data.md](docs/api/data.md)).
Sprite animation (spec 016, implemented; demo `examples/animation`): `<nnh/animation.hpp>` —
`SpriteLibrary` (one block, tag `animation`: frames, clips, markers, a state machine), a 64-byte `SpriteAnimator` per
sprite (`update()` on the frame clock, notices by callback or returned view, `draw_options()`); `<name>.ase.json`
Aseprite exports → `nnh-assetc sprite` → `load_sprite_library("<name>")` (D31; fixtures in `tests/data/aseprite/`)
([api/animation.md](docs/api/animation.md)).
Particles (spec 024, implemented, console runs done, budgets set; D39; demo `examples/particles`): `<nnh/particles.hpp>` —
`ParticleEmitter::create(desc, capacity)` (one block, 48 B per particle), one plain `ParticleDesc` (rate, bursts,
one-shots, shapes, cone/outward directions, gravity, damping, spin, colour/size keys over life, sprite frames, world or
local space), `emit`/`emit_at`, `update()` once per frame or `bind(scene)` (D30), `draw(screen[, texture])` = one
draw-list command expanded at submit under `AppConfig::gfx.max_particles` (0 by default); seeded counter-based
randomness, the same bits on PC and 3DS (self-check golden `C5EF1A84`); presets are a game-declared data type turned
into a `ParticleDesc` ([api/particles.md](docs/api/particles.md)). The platformer's dust, puffs and muzzle flashes
(`level_effects.*`) leave its replay lines unchanged.
Input actions (spec 017, implemented; demo `examples/actions`): `<nnh/actions.hpp>` —
`ActionSet` of button/axis/vector actions with up to 4 bindings each (buttons, stick directions, sticks, composites,
touch areas), contexts bound to scenes, `pressed`/`vector`/`held_frames`/repeat and `ActionPressed` events in the same
frame (D32 after-input hook), listen/conflicts/reset for a controls screen, `NNHB` overrides saved with `nnh::save`
([api/actions.md](docs/api/actions.md)).
Save data with versions (spec 027, implemented, Linux and console checks passed, budgets set; D45, D46; demo `examples/saves`):
`<nnh/saves.hpp>` — `NNH_SAVE_TYPE(Progress, "progress", NNH_SAVE_VERSION(3), NNH_SAVE_STEP(1, from_v1), ...,
NNH_SAVE_FIELD(member, id), NNH_SAVE_RETIRED(ids...))` in the struct's namespace (tables found by ADL; ids, retired ids
and a complete step chain checked at compile time), `NNH_SAVE_STRUCT`, `NNH_SAVE_SUMMARY`; `save::store`/`load` with a
`Buffer<T>` (`max_size<T>()`), `encode`/`decode`, `info`/`exists`/`remove`; a self-describing format inside spec 002's
`NNHS` container (same bytes on PC and 3DS), the whole file validated first, exact conversions, a `Report`
(unknown/dropped/adjusted, backup), the backup on any damage, `OldFields` for steps, a Debug dump and `NNH_SAVE_DUMP`;
golden files in `tests/data/saves/`, fuzz target `fuzz_saves` ([api/saves.md](docs/api/saves.md)).
Game fonts and localization (spec 029, implemented, console and Linux checks passed, budgets set; D49, D50; demo
`examples/localization`): `<nnh/font.hpp>` — `load_font("fonts/body_16")` of a `.nnhfont` the build baked from a
`<name>.font` description (TTF/OTF via vendored host-only `third_party/stb_truetype`; size, named character sets, the
characters of chosen languages' strings, outline, ≤ 3 fallbacks, a packed licence) or a BMFont `.fnt` (pages via
`third_party/stb_image`, PNG only) — `nnh-assetc font`; 4-bit coverage pages through the contract's `texture_create_alpha`;
a 32-entry refcounted table reached through `g_font_resolve` (no font code linked without `load_font`); `TextOptions::font`,
`wrap_width`, `max_lines` (`…`), `outline_color`, `gfx::measure_text(utf8, options)`, `measure_chars`, `font_metrics`;
one shared wrapper (`src/text/text_wrap.*`: spaces, CJK, kinsoku) used by gfx and the UI's pixel seam (`Ui::set_font`,
`UiFont::Slot0..7`) ([api/fonts.md](docs/api/fonts.md)). `<nnh/localization.hpp>` — `nnh_add_game(... LANGUAGES en pt-BR ja
BASE en [STRICT_STRINGS])` with `strings/<code>.strings` (or `strings/<code>/*.strings`) checked by `nnh-assetc strings` /
`strings-index` (keys, placeholders, CLDR 46 plural forms, glyph coverage by the language's font roles, `qps` in Debug) into
`strings/<code>.nnhstr` + `.nnh_loc`; `loc::start/stop`, `tr`, `format` + `arg`/`arg_key`, `set_language` (next frame, D32
hook), `follow_system`, `LanguageChanged`, `use_fonts`/`font(role)`, `apply(ui, roles)`, `dump`/`set_show_keys`;
`system::language()` (3DS CFGU, `NNH_SIM_LANGUAGE`, script command `language CODE|off`) ([api/localization.md](docs/api/localization.md),
guide [localizing-a-game.md](docs/guides/localizing-a-game.md)). Tests build tables with `tests/loc/loc_fixture.hpp`.
Camera helpers (spec 018, implemented; D33; demo `examples/camera`): `<nnh/camera.hpp>` —
`nnh::CameraRig`, a 128-byte value per view: `update(target)` once per frame after moving the target (one 60 Hz
frame, bit-identical on PC and 3DS), `apply()` writes `Camera2D`; dead zone in screen pixels, per-frame smoothing,
limits (`set_limits(map.bounds())`), seeded trauma shake, zoom about the centre, `CameraLayout::TallWorld` (both
screens as one world) or two rigs for two views, `to_world`/`to_screen`, `debug_draw()`. `Camera2D::zoom` (0.5–4)
is applied by core to sprites, text, shapes and tile layers (snapped tile edges, row chunks below zoom 1; zoom 1
byte-identical) ([api/camera.md](docs/api/camera.md), [gfx.md#zoom](docs/api/gfx.md#zoom)).
Scene transitions (spec 023, implemented, console runs done, budgets set; D38; demo `examples/transitions`):
`<nnh/transition.hpp>` — `transition::replace/push/pop/cover(scene, {.effect, .reveal, .bottom, .tall, .out, .hold,
.in, .out_ease, .loading, .on_covered, .on_finished, .publish, ...})`: out → covered (the stack change through spec
001's deferred path, `on_covered`, a loading step per frame, the hold) → in, whole 60 Hz frames, easings from
`<nnh/ease.hpp>`; effects `fade/wipe/split/blinds/iris/custom/none` as integer rectangles on layer 15 (`ignore_camera`),
per screen or across both (`tall`), `reveal_at` for the reopening iris; no scene updates during a transition unless
`update_out/update_in`; game input blocked at its source (`input::set_blocked` too; held controls wait for release);
queries `running/phase/coverage/...`; pure `effect_rects/draw_effect`. Run by a scene-stack hook (one null check);
self-check `BFE1D810` ([api/transition.md](docs/api/transition.md)). The platformer's doors use a tall iris
(`PLATFORMER_DEMO=doors`, `tests/doors.nnhs`).
3D foundation (spec 026, part 026a mono implemented, console and Linux checks passed, budgets set; 026b stereo implemented, console and Linux checks passed, budgets set; D41–D44, D47; demo
`examples/models`): `<nnh/math3d.hpp>` — `Vec3`, `Quat`, `Mat4`, `Transform3D`, `look_at`, `perspective`, `slerp`… with
the same bits on PC and 3DS ([api/math3d.md](docs/api/math3d.md)); `<nnh/mesh.hpp>` — `create_mesh`, `make_cube/plane/
sphere`, `load_mesh("models/ship")` from glTF 2.0 converted by `nnh-assetc mesh` into quantised `.nnhmesh` (fixtures in
`tests/data/gltf/`), `Material3D` ([api/mesh.md](docs/api/mesh.md), guide [models.md](docs/guides/models.md));
`<nnh/gfx3d.hpp>` — `Camera3D`, `Lighting` (ambient + 4), `gfx3d::draw` recorded like 2D and drawn between the 2D layers
at the world layer (default 8), opaque then blended back to front, culling, `to_screen`/`screen_ray`, stats, probes
([api/gfx3d.md](docs/api/gfx3d.md)); on by `AppConfig::gfx.max_draws_3d` (default 0: a 2D game links no 3D code); the
simulator draws 3D with the engine's CPU rasteriser (`engine/platforms/shared/raster3d/`), the 3DS with citro3d
(`engine/platforms/3ds/shaders/mesh3d.v.pica`).
UI widgets (spec 028, implemented, console and Linux checks passed, budgets set; D48; demo `examples/ui`, guide
[menus](docs/guides/menus.md)): `<nnh/ui.hpp>` — `Ui::create(desc)` (one block, 88 B per widget on the 3DS, roots per
screen), widgets built in code under a parent (`label` with wrap/`...`/typewriter reveal, `image`, `progress`, `panel`
(solid or 9-slice), `spacer`, `button`, `toggle`, `slider`, `choice`, `list` (a row pool filled by the game's `fill`
only for rows coming into view; `columns > 1`: a grid), `vbox`/`hbox`/`grid_box`/`stack`/`scroll_box`), `UiWidget`
generational handles, game `UiKey`s; integer layout only when dirty (stretch ratios, nine-point anchors, completed at
`draw()` for changes after `update()`); focus by D-Pad/Circle Pad (repeat 24/6, wrap on a fresh press, neighbours, a
geometric fallback, disabled skipped or inert → `Refused`, default and remembered focus), modals and `prompt`; touch on
the bottom screen; `update()` (or `bind(scene)`, D30) → `results()`/`activated(key)`/callbacks/opt-in bus events;
controls held when a UI becomes active are ignored; text sources (copy, `borrow`, `key` + resolver, `text_changed()`:
the localization seam); `UiStyle`/`UiTheme` per state (`ui::default_theme()` art-free); `draw()` = public gfx calls on
layer 14 with `ignore_camera`; `use_actions` adapter linked only when called. Self-check `30400DA6`
([api/ui.md](docs/api/ui.md)).
Live values (spec 030, implemented, console and Linux checks passed, budgets set; D51, D52; demo `examples/live`, guide
[tuning your game](docs/guides/tuning-your-game.md)): `<nnh/live.hpp>` — `live::tune(name, &value, {.min, .max, .step}[,
OnChange])` for float/int/bool/Color/enum by pointer (the value then is the default), `live::action`, `live::watch`
(read only when shown or followed) and `live::show` (per frame); RAII `Handle` (`changed()` on the one frame a change
landed, `reset()`); one block (tag `live`) at the first registration, `live::configure()`; changes from the panel and `nnh`
apply at the start of the next frame; the overlay's Tunables and Watches pages (SELECT + R) take the controls (DevPanel
blocker), edited by buttons and touch; pause and step in core (SELECT + Y / SELECT + X, the simulator's Pause/F10,
`live::pause()`), the held frame drawn again while paused; `nnh` typed commands and `.nnhs` `set`/`reset`/`action`/`dump`/
`pause`/`resume`/`step`/`expect-value`, dumps saved as `build/logs/<game>-<date>-<time>-tunes.toml`, `nnh run --tunes FILE`
/ `--hold` (a held start); Ryu's `f2s` (`third_party/ryu`) prints floats; Release: every call an empty inline
([api/live.md](docs/api/live.md)). The platformer's movement and camera numbers are tunables.
Console remote (spec 013, implemented, D28; no game API): `nnh run <game> [--pc] --script <file.nnhs>` drives a
Debug/Development game over its log connection — buttons, sticks, touch, screenshots, log checks — and exits 0/1/2;
core side `engine/core/src/remote/`, contract `nnh/platform/remote.hpp`, PC side `tools/nnh/src/remote/`; scripts in
`examples/<name>/tests/*.nnhs` ([guides/testing-on-the-console.md](docs/guides/testing-on-the-console.md)).
Hot reload of assets (spec 031, implemented 2026-11-27; Linux checks and console runs 1 and 2 passed; D53, D54): interactive `nnh run <game> [--pc]`
watches the assets folder, builds `<game>_assets`, dry-runs `<game>` (a needed rebuild is refused) and sends the changed
outputs as one batch over the remote connection (payload messages 12–17, records 13–15); the game keeps them in
`.nnh_reload/` of its data folder and applies the batch whole at one point of a frame (`engine/core/src/reload/`,
`tools/nnh/src/reload/`); `<nnh/assets.hpp>` gains `reload_count()`, `reloaded()`, `reloaded_paths()`,
`AssetsReloaded`; textures, fonts, sounds, music, meshes, sprite libraries (animators rebind) and string tables swap
behind their handles, data assets and maps are announced, tunables on reloaded data follow the file (FR-23); `--no-reload`, typed `reload [PATH]`, script lines `swap-asset`/`restore-asset`/`expect-reload`,
`nnh restore-assets <game>` ([api/assets.md](docs/api/assets.md#hot-reload), guide
[editing assets live](docs/guides/editing-assets-live.md); demo `examples/reload`, scripts `tests/check.nnhs` (sums)
`tests/errors.nnhs` (refusals) and `tests/timing.nnhs` (save → frame, bench)).
New projects and the doctor (spec 033, built 2026-11-27; review, Linux pass and console run pending; D56; no game API):
`nnh new <name> [--title] [--author] [--engine <path>] [--unique-id <hex>]` writes a 12-file game project (a scene, a
sprite, a sound, text on both screens, `tests/smoke.nnhs`, a `.cia` set up, README) from the template embedded in `nnh`
(`tools/nnh/template/`, compiled by every PC build with tests as `nnh_template_check`, its script run by the ctest
`nnh_template_smoke`); names checked, exclusive creates, all or nothing; a checkout `nnh` writes an empty pin + guard and
prints the `NNHGE_ENGINE_DIR` lines, a release `nnh` (cache entries `NNH_RELEASE_ENGINE_URL`/`_SHA256`, spec 034) its pin.
`nnh doctor [--online] [--console <ip>] [--pc]` checks About, Simulator, 3DS, Optional, Console, Downloads and This
project through a probe seam (`tools/nnh/src/doctor/`), one `fix:` line per problem, exit 0/1/2, read-only, no network
without the options. `nnh --version` prints the version line (`build_info.*`); a protocol mismatch names both versions
and the fix (`protocol_mismatch.*`). Guides: [your first game](docs/guides/your-first-game.md),
[the nnh tool](docs/guides/nnh-tool.md#a-new-project-nnh-new).
Game projects outside the repository (spec 032, D55; guide [game projects](docs/guides/game-projects.md)): a game's own
`CMakeLists.txt` adds the engine by `FetchContent` (a hash-pinned archive; `NNHGE_ENGINE_DIR` → `FETCHCONTENT_SOURCE_DIR_NNHGE`
through the project's presets for a local engine) or `add_subdirectory` (submodule, copy), then `nnh_add_game()` works from any
of its folders (helpers use `CMAKE_CURRENT_FUNCTION_LIST_DIR` and the global property `NNH_PLATFORM`); `NNH_BUILD_EXAMPLES/
TESTS/APPS/TOOLS` default to `PROJECT_IS_TOP_LEVEL`; game targets get C++20 and the compiler cache; `NNHGE_VERSION*`; `nnh`
works in the nearest folder holding `nnh.toml` or the engine repository, checks presets first and passes `NNH_HOST_CXX` and
the override in MSYS form to 3DS configures on Windows. The core is archived from a response file in 3DS presets
(`engine/core/archive/`; `scripts/check_command_length.sh`). Proof: `tests/consumer/` and the ctests labelled `consumer`
(every PC ctest) and `consumer-full` (archive, override, submodule, pruned, wrong hash, options ON).

```
engine/core/include/nnh/<module>.hpp             public API for games (app, gfx, input, debug, ...)
engine/core/src/<module>/                        core implementation (app/, gfx/, input/, debug/, core/)
engine/core/contract/nnh/platform/<area>.hpp     platform contract (app, video, input, ...)
engine/platforms/<system>/src/<area>_<system>.cpp   platform libraries: pc (SDL3), 3ds (libctru+citro2d)
engine/platforms/fake/                           scriptable fake platform for tests (nnh/platform_fake.hpp)
engine/platforms/shared/                         code shared by platforms (stdio_fs.cpp)
engine/platforms/3ds/include/nnh/3ds/            3DS extension headers (system-only features)
tests/framework/  tests/<module>/                in-house test framework, unit tests mirroring core/src
tests/platform_conformance/                      contract tests run against every platform
tests/fuzz/                                      fuzz targets (mini-fuzz in nnh_tests; libFuzzer with NNH_BUILD_FUZZERS)
tests/consumer/  tests/consumer_tools/           a game project built against the engine by ctest (spec 032) and its driver
link/                                            nnh_link (spec 011): sockets, crypto, messages, pairing, sessions; PC + 3DS
apps/nnh-link/                                   NNH Link app (spec 011): pick a PC, pair, build and run from the console
tools/nnh/                                       the nnh command (spec 010): build, run on the console, install; test scripts (013); new, doctor (033)
tools/nnh/template/                              the nnh new template (spec 033): real files, embedded into nnh, built as nnh_template_check
tools/assetc/                                    nnh-assetc (specs 012, 014): Tiled .tmj → .nnhmap, data files → NNHD, .nnh_data; host-built in 3DS presets (NNH_HOST_CXX)
tools/data_schema/                               main() of each game's data schema program (spec 014, cmake/nnh_data.cmake)
examples/<name>/                                 one small program per feature (tests/*.nnhs: its test scripts)
specs/NNN-slug/                                  spec.md, plan.md, tasks.md
docs/                                            architecture/, process/, tech/, api/, guides/
cmake/  scripts/                                 nnh_add_game(), engine options, SDL3 fetch, dkp.sh, docs tool
third_party/<library>/                           vendored, unmodified, pinned code (stb_vorbis)
```

CMake targets: `nnh::core`, `nnh::core_contract`, `nnh::platform_{pc,3ds,fake}`, and `nnh::engine`
(core + the preset's platform), which games link.

## Build

Prerequisites and setup: [docs/guides/getting-started.md](docs/guides/getting-started.md).
Maintainer's machine: WinLibs MinGW-w64 GCC + CMake + Ninja on PATH (Git Bash); devkitPro at `C:\devkitPro`.

```sh
cmake --preset pc-debug && cmake --build --preset pc-debug     # simulator (flavors: -debug, -dev, -release)
ctest --preset pc-debug                                        # unit tests
build/pc-debug/tests/nnh_tests.exe [--reverse] [name-filter...]  # tests, one line each (--reverse: other link order)
scripts/dkp.sh cmake --preset 3ds-release                      # 3DS configure (once)
scripts/dkp.sh cmake --build --preset 3ds-release              # → build/3ds-release/**/<game>.3dsx
scripts/dkp.sh cmake --build --preset 3ds-release --target dist   # → build/3ds-release/dist/<game>/ (.3dsx, .cia)
build/pc-debug/tools/nnh/nnh run hello                         # build, send to the console (HBL → Y), show its log
build/pc-debug/tools/nnh/nnh install audio                      # .cia over the network (FBI → Receive URLs)
NNH_PRESET=pc-debug build/pc-debug/tools/nnh/nnh run input --pc --no-build --script examples/input/tests/remote.nnhs   # test script in the simulator: nnh: PASS, exit 0
build/pc-debug/tools/nnh/nnh run input --script examples/input/tests/remote.nnhs   # the same on the console (HBL → Y); PNGs in build/logs/
build/pc-debug/tools/nnh/nnh serve                              # build server for NNH Link (spec 011)
NNH_LINK_DEMO=games NNH_EXIT_AFTER_FRAMES=20 NNH_SCREENSHOT=build/link.png build/pc-debug/apps/nnh-link/nnh-link.exe   # NNH Link screens
NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/shot.png build/pc-debug/examples/hello/hello.exe
SAVES_SHOT=saved NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/saves.png build/pc-debug/examples/saves/saves.exe   # SAVES_SHOT empty|saved|golden|backup; SAVES_DEMO showcase|check|bench; NNH_SAVE_DUMP=1
build/pc-debug/tools/nnh/nnh run saves [--pc] --script examples/saves/tests/check.nnhs   # golden v1-v3 sums, encode crc 11363B5C, backup, bench
ANIMATION_DEMO=check NNH_EXIT_AFTER_FRAMES=605 build/pc-debug/examples/animation/animation.exe   # showcase (default)|check|bench
PATHFINDING_SHOT=explored NNH_EXIT_AFTER_FRAMES=40 NNH_SCREENSHOT=build/path.png build/pc-debug/examples/pathfinding/pathfinding.exe   # explored|smoothed|field|partial|door-closed|door-open|big
build/pc-debug/tools/nnh/nnh run pathfinding [--pc] --script examples/pathfinding/tests/check.nnhs   # self-check 1C0B5992, doors, island, 2x2, bench
PLATFORMER_DEMO=bench NNH_EXIT_AFTER_FRAMES=90 NNH_SCREENSHOT=build/bench.png build/pc-debug/examples/platformer/platformer.exe   # level1 (default)|bonus|bench|gate|flips|replay
build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs   # tile gate on the console (Development, HBL → Y): bench: PASS/FAIL
build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs  # levels + replay on the console: checksums = PC's, 60 Hz PASS/FAIL
build/pc-debug/tools/nnh/nnh run data [--pc] --script examples/data/tests/check.nnhs        # data assets: self-check checksum = PC's
PARTICLES_SHOT=explosion NNH_EXIT_AFTER_FRAMES=40 NNH_SCREENSHOT=build/fx.png build/pc-debug/examples/particles/particles.exe   # gallery|trail|explosion|touch|local|world|full
build/pc-debug/tools/nnh/nnh run particles [--pc] --script examples/particles/tests/check.nnhs  # self-check C5EF1A84, touch bursts, bench
QUERIES_DEMO=all NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/queries.png build/pc-debug/examples/queries/queries.exe   # first|all|cast|water|touch|check|bench
build/pc-debug/tools/nnh/nnh run queries [--pc] --script examples/queries/tests/check.nnhs  # rays, touches, self-check 4ACAA177, bench
build/pc-debug/tools/nnh/nnh run transitions [--pc] --script examples/transitions/tests/check.nnhs  # self-check BFE1D810, showcase, bench
build/pc-debug/tools/nnh/nnh run platformer [--pc] --script examples/platformer/tests/doors.nnhs   # ten door irises, 60 Hz PASS/FAIL (judged in Development builds; pc-debug only reports)
TRANSITIONS_SHOT=iris NNH_EXIT_AFTER_FRAMES=40 NNH_SCREENSHOT=build/iris.png build/pc-debug/examples/transitions/transitions.exe   # fade|wipe|split|blinds|iris|diamond|curtain|tall|loading
build/pc-debug/tools/nnh/nnh run prefabs [--pc] --script examples/prefabs/tests/check.nnhs  # room, touch spawns, self-check 7BB7901E, bench
build/pc-debug/tools/nnh/nnh run hierarchy [--pc] --script examples/hierarchy/tests/check.nnhs  # yard, self-check 23DB5C36, bench
build/pc-debug/tools/nnh/nnh run models [--pc] --script examples/models/tests/check.nnhs   # 3D: self-check A3BF4EDF, probe pixels, to_screen
MODELS_DEMO=check NNH_EXIT_AFTER_FRAMES=70 build/pc-debug/examples/models/models.exe     # showcase (default)|check|bench|smoke
UI_SHOT=options NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/ui.png build/pc-debug/examples/ui/ui.exe   # title|options|list|grid|dialog|modal|theme2
build/pc-debug/tools/nnh/nnh run ui [--pc] --script examples/ui/tests/check.nnhs   # self-check 30400DA6, the showcase by buttons and touch, bench
build/pc-debug/tools/nnh/nnh run models [--pc] --script examples/models/tests/stereo.nnhs  # stereo: slider 1, both eyes, golden B3550D40
build/pc-debug/tools/nnh/nnh run localization [--pc] --script examples/localization/tests/check.nnhs  # languages, self-check 21809981, bench
LOC_SHOT=ja NNH_EXIT_AFTER_FRAMES=200 NNH_SCREENSHOT=build/ja.png build/pc-debug/examples/localization/localization.exe   # title|options|dialog|shop|gallery|ja|ru|qps|pages; LOC_DEMO=check|bench; NNH_SIM_LANGUAGE=ja
NNH_SIM_SLIDER=1 NNH_SIM_EYE=right NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/right.png build/pc-debug/examples/models/models.exe   # left|right|anaglyph
build/pc-debug/tools/nnh/nnh run live [--pc] --script examples/live/tests/check.nnhs   # live values: sets, expect-value, pause/step, self-check 7A849F13, bench
LIVE_SHOT=tunables NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/live.png build/pc-debug/examples/live/live.exe   # stats|tunables|watches|colour|paused; LIVE_DEMO=check|bench
build/pc-debug/tools/nnh/nnh run live [--pc] --tunes build/logs/<game>-<date>-<time>-tunes.toml   # start held with a dump's values
ctest --preset pc-debug -L consumer-full                       # spec 032: the consumer project in every way (one engine build each)
tests/consumer_tools/copy_project.sh /e/games/consumer pin     # a copy of it outside the repo, pinned to a local archive of this engine
scripts/check_command_length.sh build/3ds-dev                  # longest 3DS command vs. Windows' 32,767 (limit 50 %)
build/pc-debug/tools/nnh/nnh run reload [--pc] --script examples/reload/tests/check.nnhs   # hot reload: every kind swapped, sums = PC's
build/pc-debug/tools/nnh/nnh run reload [--pc]                 # then save a file in examples/reload/assets/: it reloads (RELOAD_DEMO=bench)
build/pc-debug/tools/nnh/nnh doctor [--pc] [--online]          # tools check: one fix line per problem, exit 0 ready / 1 not
build/pc-debug/tools/nnh/nnh new my-game                       # a game project in ./my-game (outside the repository); then nnh run my-game --pc
ctest --preset pc-debug -L consumer-full -R nnh_new_smoke      # nnh new end to end: local and pinned engine (~4 min)
python -I scripts/docs_tool.py check                           # docs: broken links (python3 on Linux)
scripts/check_release_strings.sh build/3ds-release/examples/hello/hello.elf   # Release has no debug strings
```

Linux checks and libFuzzer run on the homelab build VM `nnhge-build` (VM 120, off by default; the
maintainer allows any session to start/stop it and run builds; `apt`/system changes need an OK):

```sh
ssh homelab 'sudo qm status 101 | grep -q stopped && free -m && sudo qm start 120'   # never with Bazzite (101)
FUZZ="tests/fuzz_remote" scripts/vm_check.sh fast    # incremental sync + unit tests (GCC 13, ASan/UBSan) + 2 min fuzz
FUZZ="tests/fuzz_remote" scripts/vm_check.sh final   # once per spec, before the merge: full ctest, GCC 14, 32-bit, ASan, 10 min fuzz
ssh homelab 'sudo qm shutdown 120'                                                  # when done (it also idles off)
```

A spec's checks run in this order to keep the loops short: implementation complete → one independent review and one
`final` Linux pass together → fixes (re-checked with `fast`) → console runs back to back while the maintainer is there
([docs/tech/cmake.md](docs/tech/cmake.md), "build VM").

- **3DS builds go through `scripts/dkp.sh` on Windows**: devkitPro's toolchain only runs from its MSYS2.
- **GitHub CI is manual** (saves Actions minutes): verify locally before pushing and use the homelab
  build VM for Linux checks; don't trigger or wait for CI runs unless asked. The wiki workflow still
  runs on push to `main` (a few seconds).
- Read the screenshot PNG to verify simulator output without a human.
- Run on a console: Homebrew Launcher → Y, then `nnh run <game>` (builds, finds the console, shows the coloured
  log, saves it in `build/logs/`; [guide](docs/guides/nnh-tool.md)). devkitPro's 3dslink still works:
  `/c/devkitPro/tools/bin/3dslink.exe -s <file>.3dsx -a <ip>`
  (`-s` keeps the PC listening, so logs of Debug/Development builds appear in the terminal).
- Console checks: write a `.nnhs` script and run it with `nnh run <game> --script ...`; the maintainer only presses Y,
  and the verdict, log and screenshots come back to the PC (keep console tests yes/no or fully scripted).
- Platform libraries never call core (no logging there): return errors / set `platform::last_error()`.
- Build budgets: edit one game `.cpp` → simulator < 2 s, `.3dsx` < 5 s; clean engine < 30 s. Measure
  build changes before/after ([docs/tech/cmake.md](docs/tech/cmake.md)).

## Language

Code, comments, docs, skills and agents in **English** (international contributors); talk to the
maintainer in Portuguese.

## Licensing

zlib (`LICENSE`); contributions under the same license. No Nintendo SDK, leaked code or copyrighted
assets. Don't copy code from incompatibly licensed projects; reference them in `docs/REFERENCES.md`.
