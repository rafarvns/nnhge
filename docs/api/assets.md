# assets — the game's read-only files

Header: `<nnh/assets.hpp>` · Spec: [007](../../specs/007-assets-and-romfs/spec.md) · Textures:
[texture.md](texture.md) · Saves and the game's data folder: [fs.md](fs.md)

Declare the folder in the game's build: `nnh_add_game(my_game SOURCES ... ASSETS assets)` — every file
under `assets/` becomes an asset named by its relative path ([cmake.md](../tech/cmake.md#common-changes)).
Example: [`examples/sprites`](../../examples/sprites/main.cpp) (textures and a level file). Sounds and music
are assets too: `.wav`/`.ogg` files are checked by the build and loaded by path without extension
([audio.md](audio.md), [`examples/audio`](../../examples/audio/main.cpp)). Tiled maps (`.tmj`) are converted by
the build into `.nnhmap` assets of the same path (`levels/level1.tmj` → `levels/level1.nnhmap`, loaded with
`load_tilemap("levels/level1")`); Tiled's own `.tsj`, `.tiled-project` and `.tiled-session` files are not packed
([tilemap.md](tilemap.md#maps-made-in-tiled), guide [Levels in Tiled](../guides/levels.md)); a map that breaks
a rule stops the build with `file: reason`. Example: [`examples/platformer`](../../examples/platformer/main.cpp).
Aseprite sprite sheets (`<name>.ase.json` beside `<name>.png`) are converted into sprite libraries `<name>.nnhsprite`
(`load_sprite_library("<name>")`, [below](#aseprite-sprite-sheets)); Aseprite's own `.ase`/`.aseprite` files are not
packed. Example: [`examples/animation`](../../examples/animation/main.cpp).

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

## Aseprite sprite sheets

Sprite animation ([animation.md](animation.md), spec 016, D31) reads Aseprite's sprite sheet export. The suffix
`.ase.json` is reserved for it: a plain `.json` stays a data file, copied unchanged.

**Export** (File → Export Sprite Sheet, or the CLI):

- Layout: any (By Rows, Packed…); the sheet must obey the texture rules (power-of-two sides up to 1024 on 3DS: use
  the "Power of 2" constraint or a fixed width). Trim Cels is fine.
- Output: the PNG as `<name>.png` and **JSON Data** as `<name>.ase.json`, in the same folder, Hash or Array layout;
  tick **Tags** (frame tags) and **Slices**.
- CLI: `aseprite -b hero.aseprite --sheet hero.png --data hero.ase.json --format json-hash --list-tags --list-slices
  --sheet-type packed --trim`.

**What is read:**

| Export | Becomes |
|---|---|
| `frames` (file order) | the frames: `frame` (region), `spriteSourceSize`/`sourceSize` (trim offset), `duration` (ms → frame clock, rounded half up: 100 ms = 6 frames); `rotated: true` is refused |
| a tag `{name, from, to, direction, repeat}` | a clip: `forward`/`reverse` loop, `repeat` 1 → Once, n → Repeat n; `pingpong`/`pingpong_reverse` → ping-pong of `repeat` one-way passes (absent: forever), exactly as Aseprite plays it |
| a tag named `@step` or `@hit=2` | a marker (value −32768..32767) on every clip frame showing its frames, in every clip; not a clip |
| no clip tags | one clip `default`: every frame, forward, looping |
| the slice `pivot` | every frame's pivot: its first key's `pivot` (relative to the key's `bounds`) or the bottom-centre of its `bounds`; without it, top-left |
| `meta.image`, `meta.size` | must name the PNG beside the export and match its size |

Tag colours and user data, layers and other slices are ignored. The pivot point is read relative to the slice's
bounds, as Aseprite shows a slice's centre; this was not confirmed against a real export yet — if your pivots land
off by the slice's position, report it.

**Errors** stop the build with `file:line: reason`, for example:

```
art/hero.ase.json:99: frame tag "run": unknown direction "sideways" (forward, reverse, pingpong, pingpong_reverse)
art/hero.ase.json:24: frame 1 ("hero 1.aseprite"): rotated frames aren't supported
art/hero.ase.json:93: "meta": "size" is 128x16 but hero.png is 64x16
art/hero.ase.json: hero.png not found: export the sheet beside the data (--sheet hero.png)
```

Other refusals: a tag out of range, two clips with one name, a duration outside 1..65 535 ms, a clip over 256 frames
or one play over 1 hour, more than 8 markers on a frame, a pivot more than 2047 pixels away, and the library limits.
Editing one export reconverts only it (a depfile lists the JSON and the PNG).
