# texture — images on the GPU

Header: `<nnh/texture.hpp>` · Drawing: [gfx.md](gfx.md) · Design: [architecture/rendering.md](../architecture/rendering.md)

A game keeps its images as PNGs in its **assets** folder ([assets.md](assets.md)); the build checks and
converts them for each system and packs them with the game (romfs on 3DS). An image that breaks the
size rule fails the **build**, naming the file ([cmake.md](../tech/cmake.md#common-changes)). At run time
the game loads them **by path**, without the `.png`. Small games can instead embed images in the
executable with `IMAGES` (decision D19) and load them by file name; `load_texture` looks in the assets
first, then in `IMAGES`.

```cpp
// CMakeLists.txt: nnh_add_game(my_game SOURCES ... ASSETS assets)   — assets/art/hero.png
auto loaded = nnh::load_texture("art/hero");
if (!loaded) {
    NNH_LOG_ERROR(game, "%s", loaded.error().message);
    return;
}
nnh::Texture hero = std::move(loaded.value());
nnh::gfx::draw(nnh::Screen::Top, hero, {100, 80});
```

| API | |
|-----|-|
| `Result<Texture> load_texture(name)` | loads an image: from the game's [assets](assets.md) first (`"characters/hero"` → `assets/characters/hero.png`), then from `IMAGES` (file name only); load time only (allocates GPU memory) |
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
| `NotFound` | no image with that name in the game's assets or `IMAGES` |
| `InvalidArgument` | the image breaks the size rule (the message gives its size) |
| `Corrupted` | the system couldn't decode it or has no GPU memory left (details in the log) |
| `LimitReached` | `AppConfig::gfx.max_textures` textures are already loaded |
| `Unsupported` | called before `nnh::init()` |

Each failure is also logged on the `gfx` channel with the image name.

## Textures in 3D

A 3D material names a texture by value with `nnh::texture_id(texture)`; models loaded with `load_mesh` own their
textures ([mesh.md](mesh.md)). The same size rules apply.

## Hot reload (spec 031)

In Debug/Development builds run under `nnh run`, a texture loaded from an asset reloads when its PNG is saved: the same
`Texture` shows the new pixels, and `width()`/`height()` read the texture's slot so a new size shows too (Release returns
the size cached at load; `cached_width()`/`cached_height()` always do). The old GPU texture is freed once the GPU is done
with it (D47). Images embedded with `IMAGES` don't reload (they are compiled in). See
[assets.md](assets.md#hot-reload).
