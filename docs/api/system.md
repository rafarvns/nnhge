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
| `TouchScreen` | yes | not yet (mouse as touch: M1b) |
| `Stereo3D` | yes, except 2DS models | not yet |
| `CirclePad` | yes | not yet |
| `CStick`, `ZButtons` | New 3DS / New 2DS XL | not yet |

Capabilities describe the hardware; engine APIs for touch, analog input and 3D arrive in later
milestones. `capability_name(c)` returns the enum name.

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
