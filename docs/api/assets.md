# assets — the game's read-only files

Header: `<nnh/assets.hpp>` · Spec: [007](../../specs/007-assets-and-romfs/spec.md) · Textures:
[texture.md](texture.md) · Saves and the game's data folder: [fs.md](fs.md)

Declare the folder in the game's build: `nnh_add_game(my_game SOURCES ... ASSETS assets)` — every file
under `assets/` becomes an asset named by its relative path ([cmake.md](../tech/cmake.md#common-changes)).
Example: [`examples/sprites`](../../examples/sprites/main.cpp) (textures and a level file). Fonts and string files
are converted too (spec 029): a `<name>.font` description or a BMFont `<name>.fnt` becomes `<name>.nnhfont` (the
TTF/OTF sources, `.chars` files and BMFont pages are not packed; [fonts.md](fonts.md)), and with `LANGUAGES` the files
of `strings/` become `strings/<code>.nnhstr` plus the manifest `.nnh_loc` ([localization.md](localization.md)). Sounds and music
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

## Data assets

In a game with `nnh_add_game(... DATA_SCHEMA ...)` (spec 014, [data.md](data.md)), a file whose extension is a
declared key (`enemies/goblin.enemy`) is a **data asset**: the build checks the text and packs a converted binary
under the **same asset name** (the text itself is not packed), and adds one more asset, the **data index**
`.nnh_data` (the name is reserved in every game). Other files of such a game are copied as usual. Read data assets
with `nnh::data::load`, not `assets::load`. Both formats are versioned and independent of any compiler's struct
layout; their exact layout is documented in the source:

| File | Format (version 1) | Defined in |
|------|--------------------|-----------|
| a converted data asset | `NNHD`: 20-byte header (magic, version, flags, size, CRC-32 of the body, the type's schema fingerprint), then the values field by field (`u8` field index + value; unset fields absent), little-endian, ≤ 64 KiB | [`data_format.hpp`](../../engine/core/src/data/data_format.hpp) |
| `.nnh_data` | `NNHX`: 32-byte header, every data type (key, fingerprint, entries), entries (path hash, path offset) grouped data/texture/sound/music, a lookup sorted by (group, hash), the paths | [`index_format.hpp`](../../engine/core/src/data/index_format.hpp) |

Both are checked by the same bounded validators in the build and on the console (constitution X). A converted file
whose fingerprint doesn't match the game's compiled struct (an old build folder) loads as `Corrupted`: "rebuild the
assets".

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

Tag colours and user data, layers and other slices are ignored. The pivot point is relative to the slice's
bounds, as Aseprite stores and exports it (its exporter writes the key's pivot as stored; its editor draws it at the
bounds' origin plus the pivot).

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

## Models

`*.gltf` and `*.glb` files in the assets folder are converted into `<name>.nnhmesh` by the build (spec 026): load them
with `load_mesh("models/ship")` ([mesh.md](mesh.md), export guide [models.md](../guides/models.md)). A model's buffers
(`<model>.bin`, `<model>_<any>.bin` beside a `.gltf`) are not packed; `gltf`, `glb` and `nnhmesh` aren't data keys.

## Hot reload

> Status: in progress (spec 031). Implemented: the edit loop (`nnh run <game> [--pc]` watches the assets folder, builds
> the asset target and sends the changed outputs; checked in the simulator, the console run is pending), batches applied
> whole at one point of a frame, the queries and the event below, plain files, data assets, Tiled maps, textures, string
> tables, fonts, sounds, music, meshes and sprite libraries, the UI's relayout, and the script commands (`swap-asset`,
> `expect-reload`); console run 1 passed. Not yet: live values (FR-23). See [plan 031](../../specs/031-hot-reload-assets/plan.md)
> ("Implementation notes").

In a Debug or Development game run with `nnh run <game> [--pc]`, saving a file in the game's assets folder reconverts it
with the build's own steps and reloads it in the running game at the start of a frame — no restart, relink or resend
(guides: [editing-assets-live.md](../guides/editing-assets-live.md), [nnh-tool.md](../guides/nnh-tool.md#hot-reload-of-assets)). Release builds have none of it.

- **Behind your handles** (no code needed): a `Texture` loaded from an asset shows the new pixels, and `width()`/`height()`
  the new size (Debug/Development read the texture's slot; Release returns the size cached at load). A tilemap's tileset
  and UI images keep using the same texture slot, so they follow. A `Font` (and every `FontRef`) shows the new glyphs and
  metrics; a `Sound` plays its new samples (voices playing it stop, each with its `SoundFinished`, in the batch's frame);
  a `Mesh` from `load_mesh` shows the new model; music playing a reloaded file goes on at the same sample
  ([audio.md](audio.md#hot-reload-spec-031)); a `SpriteLibrary` from `load_sprite_library` answers from the new content and
  its animators rebind ([animation.md](animation.md#hot-reload-spec-031)); the current language's string chain is read again (`loc::serial()` bumped; `tr` pointers from before are invalid, as
  after a language switch; a manifest with another language list is refused: rebuild and restart). A UI updated in the
  batch's frame resolves and measures its texts again.
- **Announced** (your copies are yours): structs filled by `data::load`, a `Tilemap`, bytes from `assets::load`/`read`.
  Later reads get the new content; reload your copy when told:

```cpp
// In a scene's update(), or a subscription to nnh::AssetsReloaded:
if (nnh::assets::reloaded("levels/room")) map = std::move(nnh::load_tilemap("levels/room").value());
if (nnh::assets::reloaded("enemies/goblin")) (void)nnh::data::load("enemies/goblin", goblin);
```

| API | |
|-----|-|
| `assets::reload_count()` | batches applied since `nnh::init()` (Release: 0) |
| `assets::reloaded(path_or_name)` | the batch applied **this frame** added, replaced or removed it: an exact path (`levels/room.nnhmap`) or a load name, the path without its last extension (`levels/room`) (Release: false) |
| `assets::reloaded_paths()` | this frame's paths, as the index stores them (index files such as `.nnh_assets` included) |
| `AssetsReloaded{count, changed, removed}` | published in the batch's frame, delivered before the scene's update (never in Release) |

A batch is **all or nothing**: every file is checked by its loader's validator (maps, data files against the compiled
struct, images) and new memory is allocated before anything changes; if one file fails, nothing of the batch applies,
the game logs one line — `W assets reload refused (frame 1234): levels/room.nnhmap: corrupted (old content kept)` — and
keeps running with what it had. A data file converted for another version of its struct is refused with "made for another
struct: rebuild and restart". An applied batch logs `I assets reloaded 2 (frame 1234): art/hero, levels/room`; a removed
asset can no longer be loaded (`NotFound`) but what was loaded from it keeps its content (one warning).

Reloading is opt-in by linking: a Debug/Development game links the reload core only when it loads something reloadable
(a texture, sound, music, font, mesh, sprite library, map, data asset or string table); one that only reads plain files answers `nnh` like a game
built before spec 031 (`watching off`).

Received files are kept in `.nnh_reload/` of the game's data folder (wiped at the session's first batch and at
shutdown) and read before the packed assets; the packed assets (the romfs on the 3DS) never change. Memory
(Debug/Development only): a 16 KB receive buffer and an 8 KB overlay table (12 KB on a 64-bit PC) from the first batch, 8 bytes per texture slot,
and during a batch the old and new content of its assets at once (a batch that doesn't fit is refused cleanly).
Game logic must not depend on reloads: in Release the queries are constants.
