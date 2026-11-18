# Core and platform libraries

> Status: **implemented** (spec 002 stages 1–8): layout, CMake targets, contract areas (app, video,
> input, time, log, crash, memory, fs, system; later assets (spec 007) and audio (spec 008, decision
> D21: voices, samples and streams; the mixing rules stay in core; assets also open as streams)), the fake platform, conformance tests, capabilities
> ([docs/api/system.md](../api/system.md)) and the first extension header (`<nnh/3ds/console.hpp>`).

## The idea

NNHGE is a **portable core** plus **one library per system** that plugs into it.

```
          game
            │  <nnh/...>  (+ optional <nnh/3ds/...> extensions)
        nnh::core ─────────── platform contract (nnh/platform/*.hpp)
            ▲                        ▲ implemented by exactly one of
            │          ┌─────────────┼──────────────┬───────────────┐
            │   nnh::platform_pc  nnh::platform_3ds  nnh::platform_fake  (later: _switch, _wiiu)
            │      SDL3            libctru/citro2d    tests only
```

- **`nnh::core`**: everything portable — loop, events, scenes, logging, memory accounting, file rules,
  math. It never includes a system SDK header.
- **Platform contract**: small headers (`nnh/platform/video.hpp`, `input.hpp`, `time.hpp`, `fs.hpp`,
  `log_sink.hpp`, `crash.hpp`, `system.hpp`, ...) declaring the functions a system library must provide,
  each documented with its exact behaviour.
- **`nnh::platform_<system>`**: a static library implementing the whole contract for one system, with its
  own folder, CMake target, README, extension headers and notes.

## How it plugs in: at link time

The preset picks the platform; CMake links `nnh::core` with exactly one platform library. Calls through
the contract are plain function calls — **no virtual dispatch, no runtime registry** (consoles can't load
libraries at runtime, and each build only ever has one platform). This is the Bridge pattern resolved by
the linker ([patterns.md](patterns.md)).

Games link one target, `nnh::engine`, which is `nnh::core` + the selected platform.

## Contract rules

- **One-way dependency**: platform libraries never call `nnh::core` (not even to log). They report
  problems through return values and `platform::last_error()`; core logs them. Core → contract is the
  only direction, so the libraries link without cycles.

- The contract is the **only** thing core knows about a system. Adding a system = implementing the
  contract in a new library; core does not change.
- Every contract function documents: inputs, outputs, when it's called, what it may not do (e.g. allocate
  per frame), and the behaviour expected on failure.
- **Conformance tests** (`tests/platform_conformance/`) exercise the contract and run against the fake
  and PC platforms in CI; on 3DS they run as a test program on the console.
- A system that can't support a contract feature implements it as a documented no-op and reports the
  missing capability (below).

## System-specific features: capabilities + extensions

1. **Capability queries** in core: `nnh::system::has(Capability::Stereo3D)`, `::TouchScreen`,
   `::SecondScreen`, `::CStick`, ... plus `nnh::system::info()` (name, model, memory).
2. **Portable API when it makes sense**: e.g. 3D depth is a core API; the 3DS uses the stereo screen and
   the slider, the PC simulator shows a slider, systems without it report no capability.
3. **Extension headers** for things that only exist on one system: `<nnh/3ds/...>`, shipped by that
   platform library (e.g. reading the 3DS's step counter or play coins). Including one makes that game
   file non-portable on purpose; the build fails clearly on other platforms. Keep extension use in a
   few files of the game, behind the game's own interface.

## Folder layout

```
engine/core/include/nnh/<module>.hpp           public portable API
engine/core/include/nnh/platform/<area>.hpp    the platform contract
engine/core/src/<module>/...                   core implementation
engine/platforms/<system>/CMakeLists.txt       nnh::platform_<system>
engine/platforms/<system>/src/<area>_<system>.cpp
engine/platforms/<system>/include/nnh/<system>/...   extension headers (optional)
engine/platforms/<system>/README.md            requirements, limits, capabilities, setup
engine/platforms/shared/                       code reused by several platforms (e.g. stdio_fs.cpp: the fs
                                               contract on C stdio, used by pc and 3ds)
```

## Adding a new system (e.g. Switch)

1. Spec for the platform (toolchain, limits, capabilities).
2. `engine/platforms/switch/` implementing every contract header; presets `switch-*`.
3. Conformance tests and every example pass on it; README and `docs/tech/` page written.
4. Capability table in `docs/api/system.md` updated.
