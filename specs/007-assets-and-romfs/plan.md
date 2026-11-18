# Plan 007: Assets — build pipeline and romfs

Status: implemented (approved and implemented 2026-11-18)
Spec: [spec.md](spec.md) · Decisions D6, D19 (narrowed by this spec)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | One `<nnh/assets.hpp>`; the platform only mounts and reads; paths identical everywhere |
| II 3DS constraint | ✅ | romfs read on demand (not loaded with the program); case rules enforced on PC too (D4) |
| III Build speed | ✅ | One build step per asset; games without assets unchanged |
| IV Event-driven | ✅ | Loading is a call with a `Result`; no events |
| V No hidden costs | ✅ | Index loaded once at init; reads go to the caller's buffer or one tagged allocation |
| VI Spec + test first | ✅ | Path rules and the index are pure; the API is tested through the fake platform |
| VII Small files | ✅ | paths, index, assets API, platform readers, CMake pipeline in separate files |
| VIII Documented API | ✅ | New `docs/api/assets.md` |
| IX Clean and legal | ✅ | — |
| X Safe by default | ✅ | Read-only, confined relative paths, validated before any I/O; index size bounded |

## Design

### Public API (`<nnh/assets.hpp>`)

```cpp
namespace nnh::assets {
bool exists(const char* path);                                     // "levels/level1.txt"
Result<std::size_t> size(const char* path);
Result<std::size_t> read(const char* path, void* buffer, std::size_t capacity);   // bytes read
class AssetData {                       // move-only, memory tag "assets", freed with the object
public:
    const void* data() const; std::size_t size() const; bool valid() const; void reset();
};
Result<AssetData> load(const char* path);
}
// nnh::load_texture("characters/hero"): assets/characters/hero.png first, then IMAGES by file name.
```

Errors: `InvalidArgument` (bad path), `NotFound` (no such asset, including a case mismatch — the
message suggests the asset with the right case when one exists), `LimitReached` (buffer too small),
`IoError` (read failed), `OutOfMemory` (load), `Unsupported` (before init).

### Paths and the index (pure core logic)

- `assets/asset_path.{hpp,cpp}` — `validate_asset_path`: non-empty, at most 255 bytes, relative, `/`
  separators only, no `.`/`..` segments, no empty segments, printable ASCII (letters, digits, `-_.`,
  `/`, space). Asset file names in the folder that break these rules fail the **build**.
- `assets/asset_index.{hpp,cpp}` — the build writes `.nnh_assets` (one asset path per line, sorted,
  exact case) into the packaged folder. At init core reads it once (bounded: 64 KB, tag `assets`) and
  answers `find(path)` by binary search, **case-sensitively**; a case-insensitive scan only produces the
  "did you mean" hint. No index = the game has no assets.

### Platform contract (`nnh/platform/assets.hpp`)

```cpp
bool assets_mount();        // 3DS: romfsInit(); PC: <exe dir>/assets/ exists. false = no assets
void assets_unmount();
bool asset_size(const char* path, std::size_t& out);              // path already validated + in the index
bool asset_read(const char* path, void* buffer, std::size_t capacity, std::size_t& read);
```

Core only calls them with paths that passed validation and the index, so a platform needs no rules of
its own. **pc**: `<SDL_GetBasePath()>assets/<path>` with stdio. **3ds**: `romfs:/<path>`. **fake**:
an in-memory table set by tests (`platform_fake::add_asset`).

### Build (`cmake/nnh_assets.cmake`, used by `nnh_add_game(... ASSETS <dir>)`)

- `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` over the folder — an explicit exception to "no GLOB" (assets
  are data, not sources; adding a file must not require editing CMake), documented in `cmake.md`.
- Per file, one custom command into `<binary dir>/assets/<path>` (the folder next to the PC executable,
  and the romfs folder on 3DS): PNG → `nnh_check_image.cmake` + (3DS) tex3ds into the same name, or a
  copy (PC); other files → copy. Content keeps the source path as its name on every system.
- `nnh_asset_index.cmake` writes `.nnh_assets` (sorted list, rewritten only when it changes) and rejects
  invalid names and names differing only by case.
- A custom target `<game>_assets` builds them all; the game depends on it. 3DS: `dkp_add_asset_target`
  on that folder, passed as `ROMFS` to `ctr_create_3dsx`, so the `.3dsx` is packed after the assets.
- Files removed from the folder: the next configure (CONFIGURE_DEPENDS) drops them from the index; stale
  outputs are removed by a clean step in the index script (only files under the generated folder).

### Example

`examples/sprites`: `art/make_art.py` writes into `assets/art/`; `IMAGES` replaced by `ASSETS assets`;
textures loaded as `"art/hero"`, `"art/tiles"`; a new `assets/levels/level1.txt` describes the floating
platforms (today's pattern), read with `assets::load` and parsed by the example — screenshots must stay
identical.

### Data and memory

Index: ≤ 64 KB heap (tag `assets`; a few hundred bytes for the example). `load()` allocations: tag
`assets`. Texture loading reads the file into a temporary `assets` buffer, freed after the GPU copy.

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1, FR-2, FR-3 | build of `examples/sprites` on PC and 3DS; ctest script tests for the index (invalid names, case clashes, sorting) |
| FR-4 | `tests/assets/test_asset_textures.cpp` (fake): load by path, extension handling, IMAGES fallback |
| FR-5 | `tests/assets/test_assets.cpp`: exists/size/read/load, buffer too small, RAII release, memory tag |
| FR-6 | `test_asset_path.cpp`, `test_asset_index.cpp` (case-sensitive lookup + hint) |
| FR-7 | review: no write API; `fs` tests unchanged |
| FR-8 | the fake asset table used by all of the above |

## Risks and alternatives

- **romfs availability**: `romfsInit` fails for `.3dsx` without romfs — treated as "no assets", like
  `hello`.
- **Windows case-insensitivity**: the index catches wrong case before the file system can accept it.
- **Stale outputs** after deleting an asset: handled by the index script; worst case a clean build.
- Alternative rejected for now: the C++ `assetc` (maintainer's choice; revisit with audio/fonts).

## Constitution Check (after design)

No change; the one GLOB exception is for data folders and documented.
