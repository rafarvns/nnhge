# NNHGE — Native Nimble Homebrew Game Engine

Open-source C++ game engine that builds **native games for homebrew-enabled Nintendo consoles**,
starting with the **Nintendo 3DS**, and runs the same code in a **desktop simulator** (Windows first).

> Early development. What works today, on the PC simulator and on 3DS: the engine loop, events and
> scenes, full input, 2D graphics and text, assets, audio, collision and tilemaps (built in code or drawn in
> Tiled), typed data assets (hand-written text files checked by the build; console check
> pending), `.3dsx` and `.cia` packaging, the `nnh` tool, NNH Link, and test scripts that drive a game on the
> console from the PC. See [docs/ROADMAP.md](docs/ROADMAP.md).

```cpp
#include <nnh/app.hpp>
#include <nnh/debug.hpp>
#include <nnh/input.hpp>

int main() {
    nnh::init({.title = "My Game"});
    while (nnh::frame()) {
        if (nnh::input::pressed(nnh::Button::Start)) break;
        nnh::debug::print(nnh::Screen::Top, 8, 8, "Hello!");
    }
    nnh::shutdown();
}
```

## Goals

- **Small, well-documented API**: every public function documents cost, lifetime and platform notes.
- **Fast builds**: editing a game file rebuilds the simulator in under 2 s and the `.3dsx` in under 5 s
  (measured in the [ROADMAP](docs/ROADMAP.md)).
- **Simulator**: the game runs as a desktop app with both screens, enforcing 3DS limits.
- **Tests on the console**: scripts press buttons, touch, take screenshots and check the log, the same in
  the simulator and on a 3DS ([guide](docs/guides/testing-on-the-console.md)).
- **NNH Link**: a console app that triggers builds on your PC and runs the result
  ([guide](docs/guides/nnh-link.md)).
- **Native artifacts**: `.3dsx` for the Homebrew Launcher and `.cia` for the HOME Menu
  ([packaging](docs/guides/packaging.md)).

## Getting started

See [docs/guides/getting-started.md](docs/guides/getting-started.md).

## Documentation

- Guides: [getting started](docs/guides/getting-started.md), [the nnh tool](docs/guides/nnh-tool.md),
  [testing on the console](docs/guides/testing-on-the-console.md), [debugging on the 3DS](docs/guides/debugging-on-3ds.md),
  [NNH Link](docs/guides/nnh-link.md), [distributing your game](docs/guides/packaging.md),
  [levels in Tiled](docs/guides/levels.md), [menus with UI widgets](docs/guides/menus.md)
- API reference: [docs/api/](docs/api/)
- Architecture (event bus + scenes, patterns, structure, decisions): [docs/architecture/](docs/architecture/)
- How we work (Spec-Driven Development, TDD, SOLID, constitution): [docs/process/](docs/process/)
- Technology notes (devkitPro/3DS, CMake, C++, SDL3): [docs/tech/](docs/tech/)
- Feature specs: [specs/](specs/)
- Prior art we learn from: [docs/REFERENCES.md](docs/REFERENCES.md)

Using Claude Code? The repo ships project skills and agents (`.claude/`) that follow these rules —
see [docs/process/README.md](docs/process/README.md).

## License

[zlib](LICENSE). Use it for anything, including commercial games; credit is appreciated but not required.
NNHGE uses only the open homebrew toolchain (devkitPro); it contains no Nintendo SDK code.

NNHGE is an independent project, not affiliated with or endorsed by Nintendo. Nintendo 3DS is a trademark of
Nintendo; it is named here only to say which consoles the engine runs on.
