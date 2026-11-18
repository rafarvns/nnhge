# assets — the game's read-only files

Header: `<nnh/assets.hpp>` · Spec: [007](../../specs/007-assets-and-romfs/spec.md) · Textures:
[texture.md](texture.md) · Saves and the game's data folder: [fs.md](fs.md)

> **In progress** (spec 007): the API below works and is tested; declaring the folder in the build
> (`nnh_add_game(... ASSETS assets)`) and the PC/3DS readers arrive in the next stages.

A game keeps its content — images, levels, dialogue, tables — in an **assets folder** next to its code.
The build converts each file for the system and packs them with the game: into the **romfs** inside the
`.3dsx` on the 3DS, into a folder next to the program on PC. The game reads them **by path, when it
needs them**; nothing is loaded with the program.

```cpp
auto level = nnh::assets::load("levels/level1.txt");          // assets/levels/level1.txt
if (!level) {
    NNH_LOG_ERROR(game, "%s", level.error().message);
    return;
}
parse_level(static_cast<const char*>(level.value().data()), level.value().size());

nnh::Texture hero = std::move(nnh::load_texture("characters/hero").value());   // .../hero.png
```

| API | |
|-----|-|
| `assets::exists(path)` | the game has this asset |
| `assets::size(path)` | its size in bytes |
| `assets::read(path, buffer, capacity)` | reads it whole into your buffer; returns the bytes read |
| `assets::load(path)` | reads it whole into an `AssetData` (engine memory, tag `assets`, freed with the object) |
| `AssetData::data()`, `size()`, `valid()`, `reset()` | the bytes; move-only like `Texture` |

Reads happen at load time (they touch the SD card or the disk); don't read assets every frame.
Release `AssetData` before `nnh::shutdown()`, like any engine memory, or the leak report lists it.

## Paths

- Relative to the assets folder, with `/` (never `\`), no `.` or `..` segments: `"levels/level1.txt"`.
- Letters, digits, space, `-`, `_` and `.`; at most 255 characters.
- **Case-sensitive on every system.** The 3DS romfs is; so the simulator refuses `"Art/hero.png"` for
  `art/hero.png` even on Windows, and logs the right spelling ("did you mean ...?").

## Errors

| `ErrorCode` | When |
|-------------|------|
| `InvalidArgument` | the path breaks the rules above (nothing is read) |
| `NotFound` | the game has no such asset (or no assets at all) |
| `LimitReached` | `read()`'s buffer is smaller than the asset (the message gives the size needed) |
| `IoError` | the file is listed but couldn't be read |
| `OutOfMemory` | `load()` couldn't allocate |
| `Unsupported` | called before `nnh::init()` |

Assets are read-only. Saves and files the game writes live in its data folder ([fs.md](fs.md)).
