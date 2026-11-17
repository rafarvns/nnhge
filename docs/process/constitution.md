# Constitution

The rules every plan and every change is checked against. A plan that breaks one must say which, why,
and get the maintainer's explicit approval; otherwise it is changed.

## I. Same game code everywhere
Games only include `<nnh/...>`. No platform `#ifdef` in game code or public headers. Every feature works
on the PC simulator **and** on 3DS before it is done.

## II. The 3DS is the constraint
Design for ~64 MB usable RAM on Old 3DS (32 MB linear heap by default), 6 MB VRAM, a 268 MHz ARM11 and
the PICA200. Textures: power-of-two sides from 8 to 1024. The simulator enforces the same limits.

## III. Build speed is a feature
Budgets: edit one game `.cpp` → simulator < 2 s, `.3dsx` < 5 s; clean engine build < 30 s. Public headers
stay light; no heavy dependencies. Build changes are measured before and after.

## IV. Event-driven, scene-based
Modules and scenes communicate through public APIs (queries, commands) or the event bus (facts,
requests). Events are small trivially copyable structs, delivered at one point per frame. No module
reaches into another's internals. See [../architecture/events.md](../architecture/events.md).

## V. No hidden costs in the engine
No exceptions, no RTTI, no heap allocation in per-frame paths, no `virtual` per object. Errors are
returned; misuse is caught by `NNH_ASSERT`. See [../tech/cpp.md](../tech/cpp.md).

## VI. Spec first, test first
Every feature starts with an approved spec. Every behaviour in `engine/src/` that can run without
hardware has a unit test written before the code. Hardware-facing behaviour is verified by an example
(simulator screenshot) and on 3DS.

## VII. Small, organized files
One concept per file, `.cpp` hard limit 400 lines, one public header per module, explicit CMake source
lists. See [../architecture/project-structure.md](../architecture/project-structure.md).

## VIII. Documented API
A public API change is done only when its header comments, its `docs/api/` page and an example are
updated. Every change updates the docs it makes false ([documentation.md](documentation.md)). Docs never
show code that doesn't compile. The repository docs are the source of truth; the GitHub wiki is generated
from them and never edited by hand.

## IX. Clean and legal
zlib license. No Nintendo SDK, leaked code or copyrighted assets. Code from other projects only with a
compatible license and attribution.
