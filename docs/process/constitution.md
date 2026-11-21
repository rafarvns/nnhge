# Constitution

The rules every plan and every change is checked against. A plan that breaks one must say which, why,
and get the maintainer's explicit approval; otherwise it is changed.

## I. Same game code everywhere
Games only include `<nnh/...>`. No platform `#ifdef` in game code or public headers. Every feature works
on the PC simulator **and** on 3DS before it is done. The core is portable; each system plugs in through
its own platform library implementing the platform contract
([platforms.md](../architecture/platforms.md)). System-only features are reached through capability
queries or explicit extension headers `<nnh/<system>/...>`, never through `#ifdef`.

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
Every feature starts with an approved spec. Every behaviour in `engine/core/src/` that can run without
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

## X. Safe by default
Homebrew has no sandbox, so the engine is careful for the player: file writes only inside the game's own
folder, saves written atomically, every parser of external data bounded and fuzz-tested, no telemetry,
dependencies and CI actions pinned. Bugs are caught loudly (asserts, crash report), never silently.
See [security.md](../architecture/security.md) and [errors.md](../architecture/errors.md).

## XI. Opt-in, never in the way
Every feature above the foundation (tilemaps, tags, data assets, prefabs, animation, UI, …) is optional: a game uses
it, ignores it, or mixes it with its own code. No feature requires another one the game didn't ask for (collision
works without tilemaps, tilemaps without tags, entities without prefabs), the engine never forces a way to structure a
game, and plain C++ with the core APIs (draw, input, sound, files) is always a supported way to write one. There is no
editor: every feature works through the C++ API; files are hand-written or come from common external tools.
