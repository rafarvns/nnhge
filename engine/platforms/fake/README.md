# nnh::platform_fake — test platform

Implements the platform contract without any hardware so `nnh::core` can be unit-tested
([docs/process/tdd.md](../../../docs/process/tdd.md)). It shows nothing; it **records** what core asks
for and lets tests **script** what the system reports.

| | |
|-|-|
| Target | `nnh::platform_fake` (built with the tests on PC) |
| Test API | `#include <nnh/platform_fake.hpp>` — `reset()`, `set_held_buttons()`, `set_quit_requested()`, `set_init_fails()`, `set_time_ns()`, `advance_time()` (manual clock), `state()` |
| Recorded | init/shutdown calls, frames begun/ended, clear colors, debug texts of the current frame (up to 32), console log lines (count + last) |
| Files | `src/app_fake.cpp` · `src/video_fake.cpp` · `src/input_fake.cpp` · `src/log_fake.cpp` · `src/time_fake.cpp` · `src/control_fake.cpp` · `src/fake_internal.hpp` |

Every new contract function gets a fake implementation, plus controls/recordings when tests need them.
