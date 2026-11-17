# build — build flavors

Header: `<nnh/build.hpp>`

Every binary is built in one of three **flavors**, chosen by the preset:

| Flavor | Presets | CMake build type | Asserts | Dev tools (overlay, profiler) | Optimization |
|--------|---------|------------------|---------|-------------------------------|--------------|
| Debug | `pc-debug`, `3ds-debug` | `Debug` | on | on | none, full debug info |
| Development | `pc-dev`, `3ds-dev` | `RelWithDebInfo` | on | on | optimized + debug info |
| Release | `pc-release`, `3ds-release` | `Release` | `NNH_VERIFY` only | off | optimized |

```cpp
#include <nnh/build.hpp>

nnh::debug::printf(nnh::Screen::Bottom, 8, 220, "%s build", nnh::build::flavor_name());
```

| Symbol | Meaning |
|--------|---------|
| `nnh::build::flavor()` | `Flavor::Debug`, `Development` or `Release` (`constexpr`) |
| `nnh::build::flavor_name()` | `"Debug"`, `"Development"` or `"Release"` |
| `nnh::build::kAssertsEnabled` | `NNH_ASSERT` is active (not in Release) |
| `nnh::build::kDevToolsEnabled` | overlay, profiler, simulator controls compiled in (not in Release) |
| `NNH_FLAVOR_DEBUG` / `_DEVELOPMENT` / `_RELEASE` | exactly one is defined (by CMake on `nnh::core`) |

Rules:
- Engine headers use the flavor to compile checks and tools out of Release; that's its purpose.
- Games may *display* the flavor, but shouldn't change gameplay with it — players get Release.
- A file compiled outside the CMake build (no flavor macro) is treated as Development.

Use `*-dev` for daily work and testing on 3DS (fast and debuggable); `*-debug` when stepping through
code; `*-release` to check what players get.
