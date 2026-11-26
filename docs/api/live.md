# Live values (`<nnh/live.hpp>`)

> Status: in progress (spec 030, worktree `spec-030`): the engine side, the panel and the `nnh` script commands work in
> the simulator; the console run, `--tunes`/`--hold` and part of the tests are open
> ([plan](../../specs/030-live-values/plan.md#implementation-notes)).

Tune numbers while the game runs, on the PC simulator and on the 3DS, and watch values without `debug::printf`.
Debug and Development builds only: in Release every call is an empty inline, tunables keep their declared defaults and
`changed()` is false. Optional: a game that never calls `live::` links none of it.

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

- Kinds: `float`, `int` (32-bit), `bool`, `nnh::Color`, enums (`tune(name, &e, names)`, 1..32 names), actions
  (`live::action(name, fn, context)`).
- Names: 1..31 bytes of letters, digits, `_`, `-`, `/`; up to 3 groups (`a/b/c/leaf`); a literal is checked at compile
  time, a run-time name goes through `live::Name::runtime(buffer)` and is copied.
- Values move on the grid `min + k·step` (never drifting) and stop at `min` and `max`.
- Changes from the panel and `nnh` apply at one point: the start of the next frame, before input events and the scene
  update; the optional `OnChange` callback runs there. `Handle::reset()`, `live::reset_group()` and `live::reset_all()`
  write the defaults at once (game code).
- A `Handle` owns its entry (move-only, 4 bytes); its destructor removes it. The variable must outlive the handle.

## Watches

```cpp
nnh::live::Handle pos = nnh::live::watch("player/pos", &player.pos);   // read only when shown or followed
nnh::live::show("enemies/alive", alive);                              // per frame, no handle
```

Kinds: `bool`, 8/16/32-bit integers, `float`, `Vec2`, `Color`, text (`const char*` through a pointer, a `char` array, or
`show(name, "text")`, at most 31 bytes).

## The overlay's pages

SELECT + L (F1) shows the overlay, SELECT + R cycles Stats → Tunables → Watches. On a live page the panel takes the
controls (the game reads idle until the overlay closes; held buttons stay blocked until released): Up/Down select, A opens
a group / toggles a bool / cycles a colour channel / runs an action, Left/Right step (×10 with L or R), X resets, Y dumps,
B closes the group; touch selects rows, drags bars, taps `-`/`+`/`run`. SELECT + Y pauses or resumes, SELECT + X steps
one frame (the simulator's Pause and F10 too); while paused the last frame stays on screen and audio holds.
`live::show_panel(page, "player/jump")` and `live::pause()` do the same from code.

## `nnh` and scripts

In `nnh run <game> [--pc]`: `tunes [GROUP]`, `watches`, `get NAME`, `watch NAME [every N]`, `unwatch NAME|all`, and the
script commands below. In `.nnhs` scripts: `set NAME VALUE` (a colour quoted: `"#FFC040"`), `reset NAME|GROUP|all`,
`action NAME`, `dump` (saved as `build/logs/<game>-<date>-<time>-tunes.toml`), `pause`, `resume`, `step [N]`,
`expect-value NAME OP VALUE [~ E] [within N]`. Values are parsed once on the PC into exact bits; floats print the shortest
text that reads back to the same bits. Example: [examples/live](../../examples/live/main.cpp),
[check.nnhs](../../examples/live/tests/check.nnhs).

## Limits and memory

`live::configure({.max_tunables = 128, .max_watches = 64, .max_shown = 64})` before the first registration. One block,
tag `live`, allocated at the first registration (≈ 20 KB at the defaults on the 3DS).
