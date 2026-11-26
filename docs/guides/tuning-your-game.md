# Tuning your game

Live values (spec 030, [api/live.md](../api/live.md)) let you change numbers while the game runs — in the simulator and
on the 3DS — and keep the ones you like. Debug and Development builds only; Release has none of it.

## 1. From constants to tunables

Keep the numbers in a struct the game owns, and register them when the owner starts:

```cpp
struct Tuning { float jump = 5.5f; float gravity = 0.3f; nnh::Color sky{70, 120, 200, 255}; };
Tuning g_tuning;                                   // the game reads these as usual
nnh::live::Handle h_jump, h_gravity, h_sky;
void on_enter() {
    h_jump = nnh::live::tune("player/jump", &g_tuning.jump, {.min = 0, .max = 12, .step = 0.25f});
    h_gravity = nnh::live::tune("player/gravity", &g_tuning.gravity, {.min = 0.05f, .max = 1, .step = 0.01f});
    h_sky = nnh::live::tune("colors/sky", &g_tuning.sky);
}
```

The values in the struct are the defaults, and what a Release build uses. Game logic must not depend on a value
changing: in Release it never does (`changed()` is false). The platformer does exactly this
([examples/platformer/tuning.cpp](../../examples/platformer/tuning.cpp)); its replay checksums are the same as before.

## 2. Tuning a jump on the console

Start the game (`nnh run platformer`, or from the Homebrew Launcher), then:

1. SELECT + L shows the overlay, SELECT + R goes to the Tunables page. The panel now has the controls.
2. D-Pad Down to `player`, A opens it, Down to `jump`.
3. Right raises it by one step, L or R + Right by ten. A `*` marks a value that differs from its default.
4. SELECT + L closes the overlay: play and feel the jump. Repeat.

Touch works too: tap a row, drag a bar, tap `-`/`+`. In the simulator the mouse is the touch screen.

## 3. Watching state

```cpp
nnh::live::Handle h_pos = nnh::live::watch("player/pos", &player.pos);   // read only when shown or followed
nnh::live::show("enemies/alive", alive);                                 // every frame, no handle
```

SELECT + R again shows the Watches page. `nnh` shows them too: `watch player/pos every 10`.

## 4. Pausing and stepping

On a live page, SELECT + Y pauses (the frame stays on screen, audio holds) and SELECT + X runs one frame. Values you
change while paused apply at the next frame that runs. In the simulator, Pause and F10 do the same; in code,
`nnh::live::pause()`.

## 5. Scripts

The same names work in `.nnhs` scripts ([testing-on-the-console.md](testing-on-the-console.md)):

```text
set player/jump 6.25
set colors/sky "#FFC040"        # quoted: # starts a comment
expect-value player/jump == 6.25
expect-value spawn/alive == 9 within 60
pause
step 3
resume
```

Values are parsed once on the PC into exact bits, so the same script sets the same bits in the simulator and on the
console. See [examples/live/tests/check.nnhs](../../examples/live/tests/check.nnhs) and
[panel.nnhs](../../examples/live/tests/panel.nnhs).

## 6. Keeping a session

Press Y on the Tunables page (or type `dump`): the changed values are written to the log as TOML, and `nnh` saves them
next to the log as `build/logs/<game>-<date>-<time>-tunes.toml`:

```toml
[player]
jump = 6.25 # default 5.5
```

Paste the lines into a data asset's section (spec 014) or copy the numbers into your code. To start the next session
with them: `nnh run <game> [--pc] --tunes build/logs/<file>-tunes.toml`. The game starts held before frame 1, `nnh`
sends the values, then resumes it: the values are there from the first frame. Names the game no longer declares are
reported and skipped.

## What Release does

Every `live::` call is an empty inline: no code, no names, no registry. Tunables keep the values your struct declares.
