# Live values (`<nnh/live.hpp>`)

Tune numbers while the game runs, on the PC simulator and on the 3DS, and watch values without `debug::printf`
(spec 030, D51, D52). Debug and Development builds only: in Release every call is an empty inline, tunables keep their
declared defaults and `changed()` is false. Optional: a game that never calls `live::` links none of the module. Guide:
[tuning-your-game.md](../guides/tuning-your-game.md). Example: [examples/live](../../examples/live/main.cpp).

## Tunables

The game keeps its own variables and registers them by pointer; the value at registration is the default.

```cpp
struct Tuning { float jump = 5.5f; int count = 6; bool hitboxes = false; nnh::Color sky{70, 120, 200, 255}; };
Tuning t;
nnh::live::Handle jump = nnh::live::tune("player/jump", &t.jump, {.min = 0, .max = 12, .step = 0.25f});
nnh::live::Handle count = nnh::live::tune("spawn/count", &t.count, {.min = 0, .max = 32, .step = 1});
nnh::live::Handle box = nnh::live::tune("debug/hitboxes", &t.hitboxes);
nnh::live::Handle sky = nnh::live::tune("colors/sky", &t.sky);
if (jump.changed()) rebuild();   // true on the one frame a change landed
```

| Function | What | Notes |
|----------|------|-------|
| `tune(name, float*, FloatRange{min, max, step}, OnChange = {})` | a float | range required: min < max, step > 0, ≤ 2²⁴ steps |
| `tune(name, int*, IntRange, OnChange = {})` | a 32-bit int (`int`, or `long` where it has 32 bits) | same rules |
| `tune(name, bool*, OnChange = {})` | a bool | left, right and A toggle |
| `tune(name, Color*, OnChange = {})` | a colour | A cycles the edited channel R → G → B → A |
| `tune(name, E*, names, OnChange = {})` | an enum, values 0..N−1 | 1..32 names of 1..15 bytes; the array must outlive the handle |
| `action(name, fn, context)` | a button that calls the game | at the start of the next frame, once per press |
| `Handle::changed()` | true on the frame a panel or `nnh` change wrote different bits | asserts on a handle without an entry; Release: false |
| `Handle::reset()`, `reset_group(group)`, `reset_all()` | the defaults, written now | game code: no `changed()`, no callback |
| `configure({.max_tunables, .max_watches, .max_shown})` | capacities (128, 64, 64) | before the first registration (asserts after) |
| `pause()` | pauses at the start of the next frame | D52 |
| `show_panel(Page, select)` | shows the overlay on a page, a row selected | groups opened |

- **Names**: 1..31 bytes of letters, digits, `_`, `-`, `/`; at most 3 group levels (`a/b/c/leaf`); a literal is checked at
  compile time (errors `live_name_too_long`, `..._bad_character`, `..._empty_part`, `..._too_deep`); a run-time name goes
  through `Name::runtime(buffer)` and is copied. A name can't be used twice, nor be another name's group.
- **Stepping**: values move on the grid `min + k·step` (no drift; an off-grid value moves to its neighbouring grid point) and
  stop at `min` and `max`; colour channels at 0 and 255; enums at 0 and N−1.
- **When changes apply**: panel edits and `nnh` operations wait as pending values and are written at one point, the start
  of the next game frame (after the frame counter advances, before input events, D32 hooks and the scene update), in
  name order; `OnChange` callbacks and actions run there. While paused, edits wait (marked `~`).
- **Lifetime**: a `Handle` (4 bytes, move-only) owns its entry; its destructor removes it. The variable must outlive the
  handle and not move. A handle used after its entry ended asserts.
- **Misuse** (asserts in Debug/Development): a bad range, a default outside it, a null pointer, a name used twice or as a
  group, a full table, `configure()` after a registration, a stale handle.

## Watches

```cpp
nnh::live::Handle pos = nnh::live::watch("player/pos", &player.pos);   // read only when shown or followed
nnh::live::show("enemies/alive", alive);                              // per frame, no handle
```

Kinds: `bool`, 8/16/32-bit signed and unsigned integers, `float`, `Vec2`, `Color`, text (a `const char*` through a pointer,
a `char` array, or `show(name, "text")`; at most 31 bytes, a longer text cut and warned once). A per-frame watch not shown
for 120 frames leaves the list. Registered watches are read only on frames where the Watches page shows them or `nnh`
follows them, at the end of the frame.

## The overlay's pages

SELECT + L (F1 in the simulator) shows the overlay; SELECT + R cycles Stats → Tunables → Watches. On a live page the panel
takes the controls: the game reads idle until the overlay closes or the Stats page comes back, and a button held then
stays blocked until released (D38's blocker, `DevPanel`).

| Control | Tunables page | Watches page |
|---------|---------------|--------------|
| Up/Down | select (repeat after 24 frames, then every 6; wrap on a fresh press) | select |
| Left/Right | step (×10 with L or R held) | — |
| A | open/close a group, toggle a bool, next colour channel, run an action | open/close a group |
| B | close the enclosing group | the same |
| X / Y | reset the row (a group: all of it) / dump | — |
| SELECT + Y / SELECT + X | pause or resume / step one frame | the same |
| touch | tap a row, drag a bar, tap `-`/`+`/`run`, tap a bool's value | — |

The pages are drawn with the built-in font in the debug pass (outside `max_draws`), integer positions only: the same
pixels in the simulator and on the 3DS. While paused, the last game frame (2D, tiles, particles, the game's debug text;
not 3D) is drawn again under the overlay and audio holds; the frame clock and `frame_count()` stop.

## `nnh` and scripts

In `nnh run <game> [--pc]` (typed): `tunes [GROUP]`, `watches [GROUP]`, `get NAME`, `watch NAME [every N]`,
`unwatch NAME|all`, and every script command below. In `.nnhs` scripts:

| Command | Effect |
|---------|--------|
| `set NAME VALUE` | a number, `true`/`false`, `"#RRGGBB[AA]"` (quoted: `#` starts a comment), an enum name or index |
| `reset NAME\|GROUP\|all` | the defaults, at the next frame |
| `action NAME` | runs an action |
| `dump` | writes the changed values; `nnh` saves `build/logs/<game>-<date>-<time>-tunes.toml` |
| `pause`, `resume`, `step [N]` | `step` runs N frames (1..3600) and pauses again; while paused, `wait`, `hold`, `tap`, `drag`, `expect` and `expect-value … within` are errors |
| `expect-value NAME OP VALUE [~ E] [within N]` | OP `==` `!=` `<` `<=` `>` `>=`; `~ E` a tolerance for floats and Vec2; `within N` the first passing frame of N + 1 |

Values are parsed once on the PC into exact bits; floats print the shortest text that reads back to the same bits (Ryu's
`f2s`, [third_party/ryu](../../third_party/ryu/README.md)). Names are checked against the game's catalogue when their
part of the script is sent, so a name a scene registers later works. `nnh run <game> --tunes FILE` (a dump) starts the
game held before frame 1 and sends the values first; `--hold` only holds. An older game answers
`the game has no live values (built before spec 030)`.

## Limits, memory, cost

- One block, tag `live`, allocated at the first registration: 128 tunables, 64 registered and 64 per-frame watches,
  16 follows, 64 queued operations. Entries: ≈ 80 B per tunable on the 3DS (104 B on a 64-bit PC), 48 B per registered
  watch, 76 B per per-frame watch; ≈ 20 KB on the 3DS at the defaults. The Stats page shows `live 12/128 tun ...`.
- Nothing allocates after the block; with the overlay hidden and nothing changing, the registry is never walked.
- Measured on the PC (pc-dev, `LIVE_DEMO=bench`): `show()` × 64 0.023 ms a frame, the hidden cost with 128 tunables and 64
  watches 0.0008 ms, the Tunables page 0.033 ms of CPU, registering 100 tunables + 50 watches 0.44 ms. Console numbers:
  pending (plan.md, "Stage 10").
