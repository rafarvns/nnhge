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
| 007 | [Assets and romfs](007-assets-and-romfs/spec.md) (assets folder, conversion, romfs/PC packaging, textures and data files by path) | in progress |
| 003 | [Full input](003-full-input/spec.md) (Circle Pad, C-Stick, touch; PC keyboard, mouse and gamepad) | implemented — New 3DS run pending |
| 004 | [2D graphics](004-2d-graphics/spec.md) (textures, sprites, shapes, layers, camera; ready for 3D) | implemented, verified on 3DS |
