# texture — images on the GPU

Header: `<nnh/texture.hpp>` · Drawing: [gfx.md](gfx.md) · Design: [architecture/rendering.md](../architecture/rendering.md)

> **In progress** ([spec 004](../../specs/004-2d-graphics/spec.md)): loading by name works; declaring images
> in `nnh_add_game(... IMAGES ...)` arrives in stage 3, and the PC/3DS decoders in stages 4–5.

A game declares its images in its build; they are converted for each system and embedded in the
executable (decision D19; romfs comes with M3). At run time it loads them **by name**: the file name
without folder or extension.

```cpp
// CMakeLists.txt: nnh_add_game(my_game SOURCES ... IMAGES art/hero.png art/tiles.png)
auto loaded = nnh::load_texture("hero");
if (!loaded) {
    NNH_LOG_ERROR(game, "%s", loaded.error().message);
    return;
}
nnh::Texture hero = std::move(loaded.value());
nnh::gfx::draw(nnh::Screen::Top, hero, {100, 80});
```

| API | |
|-----|-|
| `Result<Texture> load_texture(name)` | loads an embedded image; load time only (allocates GPU memory) |
| `Texture::valid()` | loaded and usable (false when empty, reset, or after `shutdown()`) |
| `Texture::width()`, `height()` | size in pixels |
| `Texture::reset()` | frees it now (the Texture becomes empty) |

`Texture` is move-only, like `Subscription`: one owner, freed when the owner goes away. A texture freed
during a frame in which it was drawn still appears in that frame; its memory is released right after.
Handles kept after `nnh::shutdown()` are harmless (`valid()` is false, `reset()` does nothing).

## Rules (the same on PC and 3DS)

- Width and height must each be a **power of two from 8 to 1024** (8, 16, 32, …, 1024), the 3DS GPU's
  rule. Pack small images into a sprite sheet and draw regions of it ([gfx.md](gfx.md)).
- PNG transparency is kept; filtering is nearest (sharp pixel art).
- Textures live in GPU-visible memory: the `Linear` region, tag `textures` ([memory.md](memory.md)).
  A 256×256 texture takes 256 KB.

## Errors

| `ErrorCode` | When |
|-------------|------|
| `NotFound` | no image with that name in this game's `IMAGES` |
| `InvalidArgument` | the image breaks the size rule (the message gives its size) |
| `Corrupted` | the system couldn't decode it or has no GPU memory left (details in the log) |
| `LimitReached` | `AppConfig::gfx.max_textures` textures are already loaded |
| `Unsupported` | called before `nnh::init()` |

Each failure is also logged on the `gfx` channel with the image name.
