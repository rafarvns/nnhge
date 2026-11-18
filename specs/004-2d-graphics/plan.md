# Plan 004: 2D graphics — textures, sprites, shapes, camera

Status: implemented (2026-11-18); verified on PC and an Old 3DS XL
Spec: [spec.md](spec.md) · Architecture: [rendering.md](../../docs/architecture/rendering.md) · Decisions D19, D20

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | One `gfx` API; images loaded by name on every system; no `#ifdef` in games |
| II 3DS constraint | ✅ | POT 8–1024 checked at build and load; textures in linear memory, accounted; fixed draw capacity |
| III Build speed | ✅ | One custom command per image (incremental); no new dependency (tex3ds ships with devkitPro, PNG via SDL 3.4) |
| IV Event-driven | ✅ | Drawing is per-frame state in `draw()`; no events needed. Texture load failures are `Result`s |
| V No hidden costs | ✅ | Draw list and texture table allocated at `init()`; no heap or virtual per draw |
| VI Spec + test first | ✅ | Texture table, size rules and the draw list are pure core logic, unit tested first |
| VII Small files | ✅ | math, texture table, textures, draw list, draw calls, debug list, each backend part in its own file |
| VIII Documented API | ✅ | `docs/api/gfx.md` rewritten, new `texture.md`, `math.md`; `rendering.md` already written |
| IX Clean and legal | ✅ | Example art drawn for the project (zlib); tex3ds and SDL are used as tools/libraries only |

## Design

### Public API

```cpp
// <nnh/math.hpp> — small value types shared by 2D (and later 3D, which adds Vec3/Mat4 beside them)
struct Vec2 { float x = 0, y = 0; };           // + - * / operators, constexpr
struct Rect { float x = 0, y = 0, w = 0, h = 0; };
struct Region { int x = 0, y = 0, w = 0, h = 0; };   // pixels inside a texture; empty = whole texture

// <nnh/texture.hpp>
class Texture {                                 // move-only RAII handle {slot, generation}
public:
    int width() const;  int height() const;  bool valid() const;  void reset();
};
Result<Texture> load_texture(const char* name); // name = image file name without extension, as declared
                                                // in nnh_add_game(... IMAGES hero.png) → "hero"
// errors: unknown name, not a power of two / out of 8..1024, decode failure, texture table full,
//         out of linear memory

// <nnh/gfx.hpp> additions
struct DrawOptions {
    Region region{};            // sprite-sheet frame; empty = whole texture
    Vec2 pivot{};               // in region pixels; `position` is where the pivot lands
    float rotation = 0;         // radians, clockwise on screen, around the pivot
    Vec2 scale{1, 1};
    bool flip_x = false, flip_y = false;
    Color tint = colors::white; // multiplies the texture color
    float opacity = 1;          // multiplies tint alpha
    int layer = 0;              // 0..15, higher draws on top
    bool ignore_camera = false; // HUD
};
struct ShapeOptions { int layer = 0; bool ignore_camera = false; };
struct Camera2D { Vec2 position{}; };           // top-left of the view in world pixels

namespace gfx {
void draw(Screen, const Texture&, Vec2 position, const DrawOptions& = {});
void fill_rect(Screen, Rect, Color, const ShapeOptions& = {});
void draw_rect(Screen, Rect, Color, const ShapeOptions& = {});   // 1-pixel outline
void draw_line(Screen, Vec2 from, Vec2 to, Color, const ShapeOptions& = {});
void set_camera(Screen, Camera2D);  Camera2D camera(Screen);
struct Stats { int draws, max_draws, dropped_draws, textures, max_textures; std::size_t texture_bytes; };
Stats stats();
}
// AppConfig: GfxConfig gfx = {.max_draws = 2048, .max_textures = 128};
```

Calls are valid inside a frame (between `frame()` returning true and the next `frame()`); outside, they
are ignored, like `debug::print`. Final positions are rounded to whole pixels (rendering.md).

### Platform contract (`nnh/platform/video.hpp`)

```cpp
using TextureHandle = std::uintptr_t;           // platform's own id (0 = none)
struct TextureInfo { TextureHandle handle; int width; int height; std::size_t bytes; };
/// Decodes an embedded image in the platform's build format (3DS: .t3x, PC: PNG) into a GPU texture.
bool texture_create(const void* data, std::size_t size, TextureInfo& out);   // load time, may allocate
void texture_destroy(TextureHandle);                                         // after the frame using it

struct DrawItem2D {                             // already sorted, camera applied, pixels rounded
    enum class Kind : std::uint8_t { Sprite, FillRect, RectOutline, Line } kind;
    TextureHandle texture;  Region region;      // Sprite
    float x, y, w, h;                           // destination (Sprite/Rect) or from (x,y) → to (w,h) (Line)
    float pivot_x, pivot_y, rotation;  bool flip_x, flip_y;  Color color;  std::uint8_t layer;
};
void draw_2d(Screen, const DrawItem2D* items, int count);   // one call per pass; must not allocate
```

`debug_text` and `fill_rect` stay; core now calls them in the debug pass at the end of the frame.

### Core (pure parts unit tested)

- `math/` — `Vec2`/`Rect` operators (header only).
- `gfx/texture_rules.hpp` — `valid_texture_size(w, h)`: power of two, 8–1024.
- `gfx/texture_table.{hpp,cpp}` — fixed slots `{generation, TextureInfo}`, generation base kept across
  `init()` (same scheme as the subscriber table): stale handles are detected.
- `gfx/draw_list.{hpp,cpp}` — fixed array of recorded draws per screen; `record()` drops and counts on
  overflow (assert in Debug/Development, one warning per frame in Release); `build_pass()` applies the
  camera, rounds positions, resolves texture handles (stale → skipped, assert in Debug) and orders by
  layer then call with a 16-bucket counting sort into a preallocated index array (O(n), no heap).
- `gfx/textures.cpp` — `load_texture`: registry lookup → `platform::texture_create` → size rule →
  table insert → memory accounting (`Linear`, tag `textures`); `Texture` destructor defers
  `texture_destroy` to the end of the frame (the GPU may still read it).
- `gfx/draw2d.cpp` — public draw calls, camera state, stats.
- `gfx/image_registry.hpp` — `struct EmbeddedImage { const char* name; const void* data; std::size_t size; }`
  and the table the build generates for each game (always generated, empty without images; tests
  provide their own).
- `debug/debug_list.{hpp,cpp}` — debug text and overlay rectangles recorded (fixed 8 KB of text) and
  drawn in the debug pass, so they stay on top of sprites submitted at the end of the frame.
- `app.cpp` — `gfx_init` after `events_init` (capacities from `AppConfig::gfx`), end of frame: 2D pass
  per screen, debug pass, deferred texture frees, `platform::end_frame`.
- `memory` — internal `detail::mem_account(region, tag, delta)` for memory the platform allocates itself
  (citro3d/SDL textures), so `textures` shows in stats, budgets and the leak report.

### Build: images in a game

```cmake
nnh_add_game(sprites SOURCES main.cpp ... IMAGES art/hero.png art/background.png)
```

`cmake/nnh_images.cmake`, one custom command per image (only that image rebuilds when it changes):
1. `cmake -P cmake/nnh_check_image.cmake` reads the PNG header and fails with
   `hero.png is 100x60: textures need power-of-two sides from 8 to 1024` (AC-4).
2. 3DS: `tex3ds -f rgba8888 -z auto -i hero.png -o hero.t3x` (format option later). PC: the PNG itself.
3. `cmake -P cmake/nnh_embed.cmake` writes `image_hero.cpp` (a `const unsigned char[]`).
4. A configure-time `images_<game>.cpp` lists names → arrays. Duplicate names are a configure error.

### Platform work

- **pc:** `SDL_CreateGPURenderer(nullptr, window)` when a GPU device is available, else
  `SDL_CreateRenderer` (headless/offscreen); logical presentation and screenshots unchanged.
  `texture_create`: `SDL_LoadPNG_IO` → `SDL_CreateTextureFromSurface`, `SDL_SCALEMODE_NEAREST`,
  bytes = w·h·4. `draw_2d`: clip to the screen rect; sprites with `SDL_RenderTextureRotated` (source
  rect = region, center = pivot·scale, flip flags, color/alpha mod); shapes with
  `SDL_RenderFillRect`/`SDL_RenderRect`/`SDL_RenderLine`. SDL batches internally.
- **3ds:** `texture_create`: `Tex3DS_TextureImport(data, size, &tex, nullptr, false)` (linear memory),
  nearest filter, bytes = `tex->size`. `draw_2d`: `C2D_DrawImage` with a per-draw `Tex3DS_SubTexture`
  for the region (flips by swapping u/v), `C2D_DrawParams` (pos, size, center, angle), `C2D_ImageTint`
  for tint/opacity; shapes with `C2D_DrawRectSolid`, four `C2D_DrawLine` for outlines,
  `C2D_DrawLine`. `C2D_Init(max_draws + debug headroom)` instead of the default object count.
- **fake:** `texture_create` reads a tiny test format (`"FAKE"`, width, height); `draw_2d` records the
  items per screen and pass for the tests.

### Data and memory (3DS defaults)

One draw list for both screens (each draw keeps its screen): 2048 recorded draws (~72 B) + a 2048-entry
sort index (2 B) + a reusable pass buffer of 2048 items (~60 B) ≈ 136 B per draw, ≈ 280 KB of heap at
`init()` with the default, tag `gfx` (measured in stage 2; a game drawing less lowers `max_draws`).
Texture table 128 × ~32 B = 4 KB. Debug list 12 KB (static). Textures themselves: linear memory, w·h·4
bytes each (a 256×256 RGBA8 = 256 KB).

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | build of `examples/sprites`; a CMake script test runs `nnh_check_image.cmake` on good/bad PNGs |
| FR-2 | `tests/gfx/test_textures.cpp`: load ok, unknown name, table full, stale handle, memory accounted and released, deferred destroy |
| FR-3 | `test_texture_rules.cpp` + the CMake script test (AC-4) |
| FR-4 | `test_draw_calls.cpp`: region/pivot/rotation/scale/flip/tint/opacity reach `draw_2d` |
| FR-5 | `test_draw_calls.cpp`: rect, outline, line items |
| FR-6 | `test_draw_list.cpp`: layer then call order; debug pass after 2D (`test_debug_list.cpp`) |
| FR-7 | `test_draw_list.cpp`: camera subtracted, `ignore_camera`, per-screen cameras |
| FR-8 | `test_draw_list.cpp`: overflow counted and asserted; same-texture runs stay adjacent |
| FR-9 | screenshots on PC vs photos/visual check on 3DS (AC-5) |
| FR-10 | all of the above use the fake platform |
| FR-11 | review + `rendering.md`; PC renderer on a GPU device, 3DS `C3D_Tex` |
| FR-12 | `draw_2d` takes a pass slice; test that core submits per screen then the debug pass |
| FR-13 | `test_draw_list.cpp`: building a pass does not consume the list (can be built twice) |
| FR-14 | review of names (`Camera2D`, `Vec2`, `draw_2d`) |

## Risks and alternatives

- **GPU renderer on PC**: `SDL_CreateGPURenderer` needs a GPU driver (D3D12/Vulkan/Metal); old
  machines or VMs fall back to the default renderer (2D still works, logged once). Headless CI always
  uses the fallback.
- **Pixel rounding vs. rotation/scale**: rounding the pivot position keeps sprites stable; rotated pixel
  art still shows nearest-filter jaggies on both systems (expected).
- **tex3ds output** may pad or reorder in ways that differ from the PNG; the size check runs on the PNG
  before tex3ds, and the 3DS run (AC-5) compares with the simulator.
- **Executable size**: embedded images grow the `.3dsx` (fine for examples and small games; M3 moves
  them to romfs).
- **Debug text now deferred**: visible result unchanged; overlay tests updated.
- Alternative rejected: retained sprite objects (maintainer chose immediate calls); SDL_GPU directly
  for 2D (much more code now for no visible gain).

## Constitution Check (after design)

No change. The contract grows (`texture_create/destroy`, `draw_2d`) and the debug calls move to the end
of the frame; games only see additions. Heap use at `init()` ≈ 285 KB with defaults, configurable.
