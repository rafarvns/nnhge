# profile — frame profiler

Header: `<nnh/profile.hpp>` · Design: [architecture/debugging.md](../architecture/debugging.md)

```cpp
#include <nnh/profile.hpp>

void update_enemies() {
    NNH_PROFILE_SCOPE("enemies");   // times this scope
    ...
}
```

- `NNH_PROFILE_SCOPE(name)` times the enclosing scope; `name` must be a string literal (its address
  identifies the scope). Calls with the same name add up; nested scopes include their children.
- The debug overlay shows the slowest scopes of the previous frame. In code:
  `nnh::profile::slowest(out, max)` fills `ScopeStats{name, total, calls}`, slowest first.
- Up to 64 distinct scopes per frame; more are ignored. No allocation.
- Compiled out in Release (the macro expands to nothing).
- Timing uses the platform clock (3DS: ARM11 system tick, ~3.7 ns resolution).
