# NNHGE — Native Nintendo Homebrew Game Engine

Open-source C++ game engine that builds **native games for homebrew-enabled Nintendo consoles**,
starting with the **Nintendo 3DS**, and runs the same code in a **desktop simulator** (Windows first).

> Early development. What works today: the engine loop, buttons, debug text and both screens,
> on the PC simulator and on 3DS (`.3dsx`). See [docs/ROADMAP.md](docs/ROADMAP.md).

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
- **Fast builds**: editing a game file rebuilds in about a second, on PC and for 3DS.
- **Simulator**: the game runs as a desktop app with both screens, enforcing 3DS limits.
- **NNH Link** (planned): a console app that triggers builds on your PC and runs the result.
- **Native artifacts**: `.3dsx` for the Homebrew Launcher and `.cia` for the HOME Menu (planned).

## Getting started

See [docs/guides/getting-started.md](docs/guides/getting-started.md).

## Documentation

- API reference: [docs/api/](docs/api/)
- Architecture and decisions: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- Prior art we learn from: [docs/REFERENCES.md](docs/REFERENCES.md)

## License

[zlib](LICENSE). Use it for anything, including commercial games; credit is appreciated but not required.
NNHGE uses only the open homebrew toolchain (devkitPro); it contains no Nintendo SDK code.
