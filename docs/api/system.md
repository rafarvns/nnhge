# system — capabilities and system info

Header: `<nnh/system.hpp>` · Design: [architecture/platforms.md](../architecture/platforms.md)

Ask what the running system offers instead of writing `#ifdef`:

```cpp
#include <nnh/system.hpp>

if (nnh::system::has(nnh::Capability::Stereo3D)) enable_depth_effects();
nnh::SystemInfo info = nnh::system::info();   // {"Nintendo 3DS", "New 3DS XL", 152}
```

| Capability | 3DS | PC simulator |
|------------|-----|--------------|
| `SecondScreen` | yes | yes (simulated) |
| `TouchScreen` | yes | yes (mouse) |
| `Stereo3D` | yes, except 2DS models | yes (F6/F7 slider, F8 eye view) |
| `CirclePad` | yes | yes (keys or gamepad) |
| `CStick`, `ZButtons` | New 3DS / New 2DS XL | yes (keys or gamepad) |

Capabilities describe the hardware. Touch, sticks and ZL/ZR are read through [input.md](input.md); stereo 3D is
drawn by [gfx3d.md](gfx3d.md#stereo) from the slider below. `capability_name(c)` returns the enum name.

## The 3D slider

`nnh::system::stereo_slider()` (spec 026 FR-16) is the 3D slider now: 0 (down) … 1 (up), and 0 without
`Capability::Stereo3D` (a 2DS). Portable and cheap (read it every frame):

| System | Value |
|--------|-------|
| 3DS | the hardware slider (`osGet3DSliderState`) |
| PC simulator | its own slider: F6/F7 −/+ 0.1 (Shift: 0.01), `NNH_SIM_SLIDER=0..1`, remembered per game ([simulator.md](simulator.md#the-3d-slider-and-the-eyes)) |
| a test script runs (`nnh run --script`, Debug/Development) | the script's `slider V` — 0 from its start until it sets one, back to 0 on `slider off` — never the console's own slider, like the other controls ([testing-on-the-console.md](../guides/testing-on-the-console.md)) |
| interactive `nnh run` (typed `slider V`) | the typed value until `slider off`, then the console's own |

3D drawing uses it for stereo; a 2D game may use it for its own depth effects. The 3DS-only `n3ds::slider_3d()`
([3ds-extensions.md](3ds-extensions.md)) reads the hardware directly and ignores scripts.

`SystemInfo`: `name` ("Nintendo 3DS", "PC simulator"), `model` (3DS model read from the system
configuration, or the PC operating system) and `memory_mb` (approximate memory for the game).

## Extension headers (system-only features)

Features that exist on one system only live in that platform library's extension headers, e.g.
`<nnh/3ds/console.hpp>`:

```cpp
#include <nnh/3ds/console.hpp>   // 3DS builds only: other builds stop with a clear #error
float depth = nnh::n3ds::slider_3d();
bool fast_cpu = nnh::n3ds::is_new_3ds();
```

Keep extension use in a few files of your game, behind your own interface, so the rest stays portable.
All extension headers (console, network, launching another program): [3ds-extensions.md](3ds-extensions.md).
