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
| 012 | [Tilemaps and collision](012-tilemaps-and-collision/spec.md) (Tiled maps converted at build time, layers with parallax, objects and properties, moving boxes against solid and one-way tiles, overlaps) | in progress (stages 0–5 done: maps built in code, drawn, loaded from `.nnhmap` and converted from Tiled `.tmj` at build time by `nnh-assetc`; the stage-3 gate passed on the Old 3DS XL: 6 layers / 600 sprites at 60 Hz; next: animated tiles) |
| 013 | [Console remote](013-console-remote/spec.md) (the PC presses buttons, touches, takes screenshots and runs test scripts on a Development game, console or simulator) | implemented, verified on 3DS (it ran spec 012's gate) |
| 003 | [Full input](003-full-input/spec.md) (Circle Pad, C-Stick, touch; PC keyboard, mouse and gamepad) | implemented — New 3DS run pending |
| 004 | [2D graphics](004-2d-graphics/spec.md) (textures, sprites, shapes, layers, camera; ready for 3D) | implemented, verified on 3DS |
