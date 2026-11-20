# 3DS extensions — `<nnh/3ds/...>`

Headers: `<nnh/3ds/console.hpp>`, `<nnh/3ds/network.hpp>`, `<nnh/3ds/launch.hpp>` · Design:
[architecture/platforms.md](../architecture/platforms.md)

Features that exist only on the 3DS. Each header stops non-3DS builds with a clear `#error`: keep the code
that uses them in a few files of your game, behind your own interface (NNH Link does this with one
`platform_link_3ds.cpp` and an empty PC twin). Prefer `nnh::system::has(...)` ([system.md](system.md)) for
anything portable.

## Console details — `<nnh/3ds/console.hpp>`

```cpp
float depth = nnh::n3ds::slider_3d();    // 0.0 (off) .. 1.0; always 0 on 2DS models
bool fast_cpu = nnh::n3ds::is_new_3ds(); // New 3DS, New 3DS XL, New 2DS XL
```

## Network — `<nnh/3ds/network.hpp>` (spec 011)

```cpp
if (nnh::n3ds::network_start()) {   // after nnh::init(); then BSD sockets work (<sys/socket.h>)
    ...
    nnh::n3ds::network_stop();
}
```

- Starts libctru's socket service with a 1 MB buffer (heap). It doesn't wait for Wi-Fi: with Wi-Fi off,
  sockets just fail.
- Reference-counted and shared with the engine's log redirect (Development builds started with a log
  host): don't call `socInit`/`socExit` yourself. `nnh::shutdown()` stops it if the game didn't.

## Launching another program — `<nnh/3ds/launch.hpp>` (spec 011)

```cpp
const char* args[] = {"--nnh-return=sdmc:/3ds/nnh-link.3dsx"};
if (nnh::n3ds::can_launch() &&
    nnh::n3ds::launch_after_exit("sdmc:/3ds/nnh/audio.3dsx", args, 1)) {
    nnh::scenes::pop(); // end the game as usual (empty scene stack): it starts when main returns
}
```

- Uses Luma3DS's homebrew loader (`hb:ldr`), the Homebrew Launcher's own mechanism: the path becomes the
  loader's next target, then this program exits and the system starts it with
  `argv = {path, args...}` (at most 1 KB in all).
- `can_launch()` is true only for a `.3dsx` started by that loader (from the Homebrew Launcher), not for a
  `.cia`. `launch_after_exit()` returns false then, or when the path isn't a `.3dsx` on the SD card
  (`sdmc:/...`, no `..`).
- **Returning**: a game started with the argument `--nnh-return=<path>` starts `<path>` when it exits
  normally (done by the engine at `nnh::shutdown()`, unless the game called `launch_after_exit()` itself).
  NNH Link passes it so that closing a game brings you back to Link. After a crash, the console goes to the
  Homebrew Launcher as usual.
- A log host argument (`<address in hex>_3DSLINK_`, as the Homebrew Launcher's netloader sends) must be the
  **last** argument: libctru only looks there.
