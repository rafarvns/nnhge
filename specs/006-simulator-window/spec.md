# Spec 006: Simulator window — layouts, scale, fullscreen

Status: in progress (approved 2026-11-18)
Author: NNHGE maintainers · Date: 2026-11-18
Depends on: [spec 002](../002-core-foundation/spec.md) (PC platform, developer keys),
[spec 003](../003-full-input/spec.md) (mouse as touch), [spec 004](../004-2d-graphics/spec.md) (renderer).

## Problem and goal

The PC simulator always shows the two 3DS screens one above the other, at an integer scale chosen in
code (`AppConfig::pc_window_scale`), in a window the developer can only resize by dragging. On a wide
monitor most of the space is wasted, there's no way to look closely at the top screen alone or to go
fullscreen, and the window comes back at the same size every run. After this spec the developer picks a
**layout** (stacked like the console, side by side, one big screen with the other small, one screen
only), a **scale** (1×–6× or "fit the window") and **fullscreen**, with keys while the game runs, and
the simulator remembers the choice. Pixels stay sharp, the game code doesn't change, and the mouse keeps
touching the bottom screen wherever it is drawn.

## User stories

- As a game developer on a wide monitor, I want the two screens side by side, so that both are big.
- As a game developer working on the top screen, I want it large with the bottom screen small (or
  hidden), so that I can see details without a screenshot.
- As a game developer, I want fullscreen and quick scale changes with keys, so that I can playtest
  comfortably and go back to a small window to read the logs.
- As a game developer, I want the simulator to reopen with my last layout, scale and window position.
- As a contributor, I want screenshots and headless runs to keep the standard stacked layout, so that
  reference images stay comparable.

## Functional requirements

- **FR-1 Layouts.** The simulator offers at least: *stacked* (today's, console-like: top above a centered
  bottom), *side by side* (top left, bottom right, aligned to the top), *large top* (top screen big, bottom
  screen small beside it), *top only* and *bottom only*. A developer key cycles through them while the
  game runs.
- **FR-2 Scale.** Integer scales 1× to 6× and *fit*: the largest integer scale that fits the window (or
  the monitor in fullscreen), centered, with the rest filled by a neutral background. Keys step the scale
  up/down and toggle fit. Pixels stay square and sharp at every scale.
- **FR-3 Fullscreen.** A key toggles borderless fullscreen on the current monitor (with *fit* scale while
  fullscreen) and back to the previous window size.
- **FR-4 Touch follows the layout.** The mouse touches the bottom screen wherever and however large the
  layout draws it; in *top only* there is no touch. Positions stay in 3DS pixels (spec 003).
- **FR-5 Nothing changes for games.** Layouts are a view: screen sizes, coordinates, `gfx`, `input` and
  `debug` behave exactly the same; no new public game API is needed (`AppConfig::pc_window_scale` keeps
  working as the initial scale).
- **FR-6 Remembered settings.** Layout, scale/fit, fullscreen and window position/size are saved when
  the simulator closes and restored on the next run (per game), in the PC's user settings — never in
  the game's data folder or saves. A broken or missing settings file falls back to the defaults.
- **FR-7 Reproducible screenshots.** `NNH_SCREENSHOT` and headless runs (`NNH_EXIT_AFTER_FRAMES`,
  `SDL_VIDEO_DRIVER=offscreen`) use the stacked layout at the initial scale (`AppConfig::pc_window_scale`,
  default 2 — today's images) unless a layout is chosen explicitly (e.g. an environment variable), and
  ignore remembered settings, so reference images don't depend on
  the developer's preferences. F12 screenshots keep capturing what is on screen.
- **FR-8 Discoverable.** The current layout and scale show briefly when changed (window title or a short
  on-screen note), and the keys are listed in `docs/api/simulator.md`. Release builds keep layouts and
  scale keys (they are about viewing, not debugging); the developer-only keys stay as they are.

## Acceptance criteria

- **AC-1** Unit tests (pure layout code) cover every layout's screen rectangles at several scales and
  window sizes (fit, centering, integer-only), mouse → bottom-screen conversion per layout (including
  *top only* = no touch), and settings parsing (valid, missing, corrupted, out-of-range values).
- **AC-2** Screenshots of `examples/sprites` in each layout (chosen through the environment) show the
  screens where expected; the default headless screenshot is byte-identical to today's.
- **AC-3** The maintainer cycles layouts, scales and fullscreen in a running game, touches the bottom
  screen in each layout, closes and reopens to find the settings restored.
- **AC-4** `docs/api/simulator.md` documents layouts, keys, settings file and screenshot behaviour;
  build budgets unchanged; the 3DS build is unaffected.

## Non-goals

- The 3D slider and stereoscopic preview (with stereo rendering, M8).
- Rotated or vertical "handheld" layouts, a physical-size mode, CRT/LCD filters, bezels.
- Several windows (one per screen), recording video.
- Remapping the developer keys.

## Constraints

- Constitution I: layouts live entirely in the PC platform library; no game or core API changes.
- Constitution II / D4: the simulator must not show more than the console: same screen sizes, no
  smoothing (integer scales, nearest filtering).
- Constitution X: the settings file is read defensively (bounded, validated) and lives outside the
  game's data folder.
- Constitution VII: layout math (pure, tested) apart from SDL code.

## Open questions

Answered by the maintainer (2026-11-18): the proposed options, all accepted.

- [x] **Keys** — F2 cycles layouts, F3/F4 scale down/up, F5 toggles fit, F9 (and Alt+Enter) fullscreen;
  F1, F10, F11, F12 and Pause keep their roles.
- [x] **Large top** — the bottom screen at half the top's scale (at least 1×).
- [x] **Settings location** — SDL's per-user preferences folder, one file per game (not the game's data
  folder).
- [x] **Release builds** — layout, scale and fullscreen keys work in Release too (viewing options).
- [x] **Change notice** — in the window title only, for a moment.

Note (plan, 2026-11-18): FR-7 originally said "stacked at 1×"; today's reference screenshots are at the
default initial scale 2, so headless runs use the initial scale to keep them byte-identical (AC-2).
