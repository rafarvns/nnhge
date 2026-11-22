# Specs

Every feature starts here. Process: [docs/process/sdd.md](../docs/process/sdd.md).

To start a new spec, copy `_templates/` to `NNN-<slug>/` with the next free number (or ask the
`nnh-spec-writer` agent).

| # | Spec | Status |
|---|------|--------|
| 001 | [Event bus and scenes](001-event-bus-and-scenes/spec.md) | implemented |
| 002 | [Core foundation](002-core-foundation/spec.md) (core + platform libraries, time, logs, errors, memory, debug tools, file safety) | implemented, verified on 3DS |
| 005 | [Text: the built-in NNHGE font](005-text-and-builtin-font/spec.md) (same pixels on every system, UTF-8 Latin-1, measuring, debug text) | implemented, verified on 3DS |
| 006 | [Simulator window](006-simulator-window/spec.md) (layouts, scale, fullscreen, remembered settings) | implemented |
| 007 | [Assets and romfs](007-assets-and-romfs/spec.md) (assets folder, conversion, romfs/PC packaging, textures and data files by path) | implemented, verified on 3DS |
| 008 | [Audio](008-audio/spec.md) (sound effects, streaming music with loop points and fades, mixer groups, silent fallback) | implemented, verified on 3DS |
| 009 | [Packaging](009-packaging/spec.md) (icon, metadata, installable `.cia` with banner and jingle) | implemented, verified on 3DS |
| 010 | [The `nnh` tool and `nnh run`](010-nnh-cli-run/spec.md) (build, send to the console, readable saved logs, install `.cia`; first half of NNH Link) | implemented, verified on 3DS |
| 011 | [NNH Link](011-nnh-link/spec.md) (build and run from the console: `nnh serve`, the console app, pairing, launch and return) | implemented |
| 012 | [Tilemaps and collision](012-tilemaps-and-collision/spec.md) (Tiled maps converted at build time, layers with parallax, objects and properties, moving boxes against solid and one-way tiles, overlaps) | implemented (2026-11-22); the tile gate passed on the Old 3DS XL (4 layers + 300 sprites at 60 Hz, limits 6 layers / 600 sprites) — pending with the maintainer: T7.4 (`level.nnhs` on the console); Linux, sanitizers and fuzzing done |
| 013 | [Console remote](013-console-remote/spec.md) (the PC presses buttons, touches, takes screenshots and runs test scripts on a Development game, console or simulator) | implemented, verified on 3DS (it ran spec 012's gate) |
| 014 | [Data assets](014-data-assets/spec.md) (typed game data in hand-written TOML-subset files, shaped by a C++ struct, checked and converted by the build, loaded by path, references to other data and to media; optional) | approved (2026-11-22, maintainer pre-approved recommendations); [plan](014-data-assets/plan.md) and [tasks](014-data-assets/tasks.md) approved; in progress, stages 0–5 done |
| 015 | [Tweens and timers](015-tweens-and-timers/spec.md) (animate float/Vec2/Color with standard easings, delays, loops, yoyo, sequences and parallel groups; "after"/"every" timers and a plain timer value; frame-deterministic, no allocation while playing, scene-bound sets cancelled on exit; optional) | approved (2026-11-22, maintainer pre-approved recommendations); [plan](015-tweens-and-timers/plan.md) and [tasks](015-tweens-and-timers/tasks.md) approved (D30: scene attachments), not started |
| 016 | [Sprite animation](016-sprite-animation/spec.md) (sprite sheet frames, named clips with per-frame durations and loop/once/repeat/ping-pong, a small animator value with queue, speed and flips, frame markers, a small state machine; Aseprite JSON exports converted by `nnh-assetc`; frame-deterministic, no allocation while playing; optional) | approved (2026-11-22, maintainer pre-approved recommendations); [plan](016-sprite-animation/plan.md) and [tasks](016-sprite-animation/tasks.md) approved (D31: Aseprite exports → `.nnhsprite`, `.ase.json` reserved; frame clock moves to `time.hpp`), not started |
| 017 | [Input actions](017-input-actions/spec.md) (named button/axis/vector actions with several bindings: buttons, stick directions, whole sticks, composites, touch areas; contexts that follow scenes; queries, repeat and events in the same frame; player remapping with listen, conflicts and reset; overrides saved with `nnh::save`; Old 3DS fallbacks; reads the same input sample as remote/scripted input; optional) | approved (2026-11-22, maintainer pre-approved recommendations); [plan](017-input-actions/plan.md) and [tasks](017-input-actions/tasks.md) approved (D32: after-input hook list in the frame loop, `NNHB` bindings overrides), not started |
| 003 | [Full input](003-full-input/spec.md) (Circle Pad, C-Stick, touch; PC keyboard, mouse and gamepad) | implemented — New 3DS run pending |
| 004 | [2D graphics](004-2d-graphics/spec.md) (textures, sprites, shapes, layers, camera; ready for 3D) | implemented, verified on 3DS |
