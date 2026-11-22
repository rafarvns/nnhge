# app — lifecycle and main loop

Header: `<nnh/app.hpp>`

| Function | Purpose |
|----------|---------|
| `bool nnh::init(const AppConfig& = {})` | Start the engine. Returns false on failure: exit the game. |
| `bool nnh::frame()` | Present the previous frame, wait for vsync, read input, start a new frame, deliver queued [events](events.md). False = must exit. |
| `void nnh::shutdown()` | Present the open frame and release everything. Idempotent. |
| `std::uint64_t nnh::frame_count()` | Frames started since `init()`; the first frame is 1. |

`AppConfig` fields: `title` (PC window title; default: `TITLE` from `nnh_add_game`, which consoles use too),
`pc_window_scale` (initial simulator window size, default 2×) and `data_folder` (name of the game's
data folder for saves and logs; default derived from the title, see [fs.md](fs.md)) and `events`
(event bus capacities: `.events = {.queue_bytes = 32 * 1024, .max_subscribers = 512}`, see
[events.md](events.md#capacities)) and `gfx` (2D capacities: `.gfx = {.max_draws = 4096, .max_textures = 256}`,
and `.max_tiles` for [tilemaps](tilemap.md#drawing), 0 by default, ~416 per full 16×16 top-screen layer; see
[gfx.md](gfx.md#capacities)) and `audio` (`.audio = {.voices = 16, .max_sounds = 128}`, see
[audio.md](audio.md#capacities-and-errors)).

## The loop

```cpp
int main() {
    if (!nnh::init({.title = "My Game"})) return 1;
    while (nnh::frame()) {
        // 1. read input   2. update the game   3. draw
    }
    nnh::shutdown();
}
```

- Everything drawn between two `frame()` calls is one frame. Drawing outside a frame is ignored.
  Draws are recorded and reach the screen when the frame ends (the next `frame()` or `shutdown()`).
- The loop runs at **60 Hz** on every platform (vsync on 3DS; paced on PC even with 144 Hz monitors).
  For game logic at a fixed rate use `nnh::time::FixedStep` ([time.md](time.md#fixedstep)).
- `frame()` returns false when the system asks the game to close: HOME Menu → Close on 3DS,
  window closed on PC. Always leave the loop and call `shutdown()`; never `exit()` from inside a frame.
- `break` out of the loop to quit on your own (e.g. START). `shutdown()` presents the last frame.
- Games organized in scenes can use `nnh::run(first_scene)` instead of the loop ([scene.md](scene.md)).

## Platform notes

- **3DS**: `init()` sets up the GPU (citro3d/citro2d). Title and icon come from the SMDH metadata.
- **PC**: one window shows both screens. See [simulator.md](simulator.md).
