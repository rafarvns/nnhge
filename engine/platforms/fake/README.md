# nnh::platform_fake — test platform

Implements the platform contract without any hardware so `nnh::core` can be unit-tested
([docs/process/tdd.md](../../../docs/process/tdd.md)). It shows nothing; it **records** what core asks
for and lets tests **script** what the system reports.

| | |
|-|-|
| Target | `nnh::platform_fake` (built with the tests on PC) |
| Test API | `#include <nnh/platform_fake.hpp>` — `reset()`, `set_held_buttons()`, `set_quit_requested()`, `set_init_fails()`, `set_time_ns()`, `advance_time()` (manual clock), `run_expecting_crash()`, `last_crash()`, `simulate_cpu_fault()`, `set_region_capacity()`, `fail_next_writes()`, `corrupt_file()` (in-memory files, kept across init until `reset()`), `set_capabilities()`, `request_overlay_toggle()`, `add_asset()`, `set_audio_available()`, `finish_voice()`, `sample_frames()`, console remote (spec 013): `remote_connect()`, `remote_push()`, `remote_disconnect()`, `remote_sent()`, `remote_clear_sent()`, `set_remote_capture_fails()`, `remote_capture_pixel()` (the synthetic screen pattern); `state()` (incl. `vram_writes_by_core`, `voices`, `live_samples`, `remote_receive_calls`, `remote_last_read`, `remote_capture_held`) |
| Recorded | init/shutdown calls, frames begun/ended, clear colors, debug texts of the current frame (up to 32), console log lines (count + last), 2D draws, asset reads, audio voices (sample, params, loop, pause, starts/stops) and live samples, every byte core sent to the remote PC, remote reads and captures |
| Files | `src/app_fake.cpp` · `src/assets_fake.cpp` · `src/audio_fake.cpp` · `src/video_fake.cpp` · `src/input_fake.cpp` · `src/log_fake.cpp` · `src/memory_fake.cpp` · `src/system_fake.cpp` · `src/time_fake.cpp` · `src/control_fake.cpp` · `src/crash_fake.cpp` · `src/fs_fake.cpp` · `src/remote_fake.cpp` · `src/fake_internal.hpp` |

Every new contract function gets a fake implementation, plus controls/recordings when tests need them.
