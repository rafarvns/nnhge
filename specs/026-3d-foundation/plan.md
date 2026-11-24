# Plan 026: 3D foundation

Status: approved 2026-11-24 (overnight pre-approval); **part 026a implemented 2026-11-24** (console run T10.1/T10.2 and Linux VM T9.3 pending)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md)
Decisions: [D41, D42, D43, D44](../../docs/architecture/decisions.md) (written with this plan; design until implemented)

This is the largest spec so far. It is planned as **two parts with fourteen stages**; every stage ends with all tests
green on PC and the 3DS build compiling, and leaves something usable, so work can stop after any stage:

- **026a — 3D, mono, everywhere** (stages 0–10): math, meshes (code and glTF), the 3D pass among 2D layers, the CPU
  rasteriser (fake + PC), the citro3d backend, the example, the bench and the console run, all with one eye.
- **026b — stereo** (stages 11–13): the portable slider, simulator keys and eye views, the `.nnhs` `slider` command and
  right-eye screenshots, per-eye cameras (off-axis), per-eye 2D and debug passes, console stereo checks.

Which acceptance criteria each part closes is in [AC coverage per part](#ac-coverage-per-part). The implementation
starts **after spec 022 is complete on `main`** (it needs `nnh::sin_cos`, `<nnh/transform.hpp>`,
`engine/core/src/transform/sin_cos.cpp` and `src/core/det_sine.hpp`), in a worktree branch `spec-026` (see
[Merge notes](#merge-notes)).

## Decisions taken (overnight, to review)

The spec's decisions and open questions were approved as recommended. These plan-level choices were taken with the
recommended option under the overnight pre-approval; each is cheap to change before its stage starts.

| # | Decision | Why / alternative |
|---|----------|-------------------|
| P1 | Split into **026a (mono) and 026b (stereo)** in this plan; one spec, one ROADMAP item, status `implemented` only after 026b | Stereo touches the slider, the remote protocol, the simulator window and the debug pass; mono 3D is useful (and console-verified) on its own. Alternative: one sequence (more risk of a half-done stage at the end of a night). |
| P2 | **Three modules, three public headers**: `<nnh/math3d.hpp>` (values), `<nnh/mesh.hpp>` (meshes, materials), `<nnh/gfx3d.hpp>` (camera, lights, drawing) | One public header per module (VII), a 2D game that wants only vectors includes no graphics, the converter shares the mesh module's pure files. Alternative: one `gfx3d.hpp` (> 300 lines). |
| P3 | Names (D44): `Vec3`, `Vec4`, `Quat`, `Mat4`, `Transform3D`, free functions `dot`, `cross`, `normalize`, `quat_axis_angle`, `quat_euler`, `look_rotation`, `slerp`, `to_mat4`, `inverse`, `inverse_affine`, `transform_point`, `transform_direction`, `look_at`, `perspective`, `orthographic` | Overloads beside spec 022's `transform_point(Affine2D, Vec2)`/`inverse(Affine2D, Affine2D&)`; raylib/glm-like names. |
| P4 | Quaternion → matrix divides homogeneous numerators by `|q|²` (9 IEEE divisions); `slerp` uses Eberly's polynomial (no `acos`/`sin`); normalise/invert refuse magnitudes < 2⁻¹⁰⁰ by an exponent test | Makes quarter turns map axes to axes **exactly** although `sin_cos(π/4)` is not √½ (see [Math](#math-d44)); keeps FR-3's no-libm rule; subnormals (flushed by the 3DS VFP) never decide a result. |
| P5 | Vertex layout per mesh: **12, 16 or 20 bytes** (position int16×4 + normal int8×4, + uv int16×2 if any submesh is textured, + colour u8×4 if the model has colours); missing attributes are citro3d fixed attributes | ≤ 20 bytes (FR-4) and 12 for untextured, colourless meshes; one shader. Alternative: always 20 (simpler, +67 % for plain meshes). |
| P6 | Mesh table: **256 slots** fixed (16 B each), ≤ **8 textures** and ≤ 32 submeshes per mesh; meshes need 3D on (`max_draws_3d > 0`), else `Unsupported` | No new `AppConfig` field; textures owned by a mesh are ordinary textures (they count in `max_textures`). |
| P7 | A draw has **one** material override (all drawn submeshes) and one tint; per-submesh overrides = one draw per submesh | Keeps a record at 88 bytes (≤ 96, FR-26) with no per-frame pool; FR-9's "replace any submesh's material" holds through `options.submesh`. |
| P8 | Materials name textures by value, `TextureId` (`slot`, `generation`, from `gfx3d::texture_id(texture)`), not by pointer | Trivially copyable, survives moving the `Texture`, stale ids detected like 2D handles. `nnh::TextureRef` is taken by data assets (spec 014). |
| P9 | Shininess: **8 levels**, exponents 2, 4, 8, …, 256 (`2^(level+1)`); point lights: falloff `(1 − d/range)²`, **one shared 256-entry distance table** scaled by `1/range` per light | Open question answered from citro3d's `C3D_LightLut` (256 entries) and `C3D_LightLutDA` (bias/scale per light). PC and 3DS fill the tables from the same pure generator. |
| P10 | Lights are given in **world space**; core converts them to view space per eye | The game places lights like objects; the PICA wants eye space. |
| P11 | Culling at submit from a **world sphere computed at record** (16 B per draw, a hot array); in stereo a draw is kept if it is inside **either** eye's frustum; blended draws sorted **once per frame by the centre camera** | Uses the camera at the end of the frame (as 2D does) while touching the cold record only once; the same order in both eyes. |
| P12 | Stereo defaults: focal distance **5**, strength 0 = derived = `focal / 20` (≈ 10 px of disparity at infinity with a 60° view at slider 1); default camera at (0, 0, 5) looking at the origin, 60°, near 0.1, far 100 | The open question "stereo defaults" is answered provisionally; stage 13 confirms on the Old 3DS XL (T13.6). |
| P13 | `world_layer` default **8** as the spec says; the docs and the header warn that 2D at layers 0–7 (text's default 0) is drawn **under** the 3D world when a frame has 3D | Spec-fixed; the alternative (default 0 = 3D under every 2D layer) would hide skies; flagged for the maintainer. |
| P14 | gfx reaches the 3D module only through **one hook pointer**, `detail::g_gfx_3d_ops` (Optional frame hook pattern, set by a registrar in a 3D object file) | FR-31: a 2D game links no 3D code; cost for `hello` = a few null checks. **AC-9 is read as "no 3D code; at most the hook's null checks"**: budget ≤ 128 B of `.text` on 3DS Release, ≤ 1 KB on PC (the simulator's slider and eye keys live in its always-linked window code, FR-16), each measured and recorded. |
| P15 | PC composition: when the frame drew 2D below the world, the eye target is **read back** (`SDL_RenderReadPixels`) into the rasteriser's colour buffer; the finished 3D image is drawn back **opaque** (`SDL_BLENDMODE_NONE`) | Byte-identical PNGs on every renderer (FR-23) because every blend happens on the CPU. Without 2D below, the buffer starts as the clear colour (no read-back). Alternative: premultiplied composition by SDL (cheaper, but blended 3D over 2D would round per GPU). |
| P16 | PC textures for 3D: a **texel cache keyed by (slot, generation)**, filled by reading a texture back once (render to a target, read pixels) at its first 3D use | `texture_pc.cpp` (always linked) stays unchanged; the key is unique for a texture's lifetime, so no destroy notification is needed. |
| P17 | Rasteriser: shared static library `engine/platforms/shared/raster3d/` (PC and fake), compiled `-O2 -ffp-contract=off` in **every** flavour; 1/16-px sub-pixel fixed-point edges (int64 setup), top-left rule, guard band ±2048 px, near/far clipping, float depth, 8-bit integer blending, the PICA's lighting tables emulated | Deterministic and usable in Debug; Giesen's rules. |
| P18 | FR-28's debug draw records `gfx::draw_line` on layer 15 ignoring the camera (Debug/Development only), not into the debug pass (which has only text and rectangles) | No change to the debug module; documented ("lines count against `max_draws`"). |
| P19 | Probe pixels: a Debug/Development API `gfx3d::request_probes(screen, eye, points, n)` / `probe_results(out, n)`; read after `end_frame()` through the remote read-back contract, answered the next frame | The game shows its own verdict (spec decision (a)) on the console, where a frame can't be read while it is drawn. Release: does nothing. |
| P20 | Remote: message type **7 `Slider`** (thousandths, or "release"); `screenshot NAME eyes` adds the right eye as screen 2 → `NAME-top-right.png`; protocol version stays 1 (additive) | Answers the open question on the right-eye screenshot. An older game rejects the new type loudly (`Reject::Type`). |
| P21 | Simulator keys: **F6/F7** slider −/+ 0.1 (Shift: 0.01), **F8** eye view Left → Right → Anaglyph; env `NNH_SIM_SLIDER=0..1`, `NNH_SIM_EYE=left|right|anaglyph`; both remembered in `window.ini`, ignored by scripted runs (`NNH_REMOTE` forces the left view); the PC reports `Capability::Stereo3D` | F6–F8 are the free function keys in spec 006's map. |
| P22 | 3DS: two top targets `RGBA8 + DEPTH24_STENCIL8` (left replaces citro2d's DEPTH16 target, right only in 026b); 2D in a 3D game is drawn with **depth test off, colour writes only**; the command buffer grows by 640 B per `max_draws_3d`; texture filter/wrap set per draw and restored after the pass | No mid-frame depth clears (the frame-start `C2D_TargetClear` clears depth to 0); citro2d's own `GEQUAL` writes would otherwise hide 3D. |
| P23 | FR-24 tolerance proposal: probes ≥ 2 px from any projected edge; per channel ±2 unlit flat and nearest-textured, ±3 blended, ±10 lit | From the PICA's 8-bit combiners and its LUT interpolation; the first console run confirms or sets them (spec policy). |
| P24 | `gfx3d::draw` on `Screen::Bottom` asserts and does nothing (follow-up 1); every function still takes the screen | FR-12. |
| P25 | glTF buffers must sit beside the model and be named `<model>.bin` or `<model>_<any>.bin`; those are pipeline-owned (not packed); `gltf`, `glb`, `nnhmesh` are not data keys | Blender's default export; other `.bin` files stay game data (D29-style routing, D42). |
| P26 | Example `examples/models`, art and models generated by `examples/models/art/make_models.py` (committed outputs) | Original content (IX); the generator is a dev-time script like `make_turret.py`. |

## AC coverage per part

| AC | 026a closes | 026b closes |
|----|-------------|-------------|
| AC-1 | math, meshes, materials, recording, world-layer split, order, culling (mono), lights, `to_screen`/`screen_ray`, misuse, Linux VM | per-eye matrices, off-axis, focal plane, slider 0 → one eye, orthographic stereo, stereo culling union |
| AC-2 | all (the golden includes mono projections; stereo matrices join it in 026b with a **second** golden, `3d-stereo`, so 026a's never moves) | stereo golden |
| AC-3, AC-4, AC-5, AC-9 | all | AC-5: right-eye and anaglyph screenshots of the PC; AC-9 re-measured |
| AC-6 | showcase, check, bench in mono; screenshots left eye, orthographic, debug draw | right eye with slider 1, anaglyph |
| AC-7 | math golden, mono probe scenes, `to_screen` lines, screenshots, console + `--pc` | slider command, stereo probe scene (both eyes), right-eye screenshot |
| AC-8 | throughput, draws, CPU alone, fill — mono; 2D unchanged | the stereo targets (6 000 triangles, draws < 7 ms) |
| AC-10 | API pages, export guide, rendering.md, sdl3.md, devkitpro-3ds.md, D41–D44 confirmed | stereo sections, simulator keys, 3ds-extensions, remote guide |

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | Every function on PC, 3DS and fake; the slider is portable (`system::stereo_slider()`); no `#ifdef`. Speed isn't simulated (spec: "Under pressure"), the console bench guards it. |
| II 3DS constraint | ✅ | VRAM +576 000 B in 026a, +960 000 B total (top 3D targets 1 536 000 B), only with 3D on; quantised vertices in linear memory; textures keep the 8–1024 rule; the simulator enforces mesh limits and capacities. |
| III Build speed | ✅ | Light headers (math3d: `math.hpp` + `transform.hpp`; gfx3d/mesh: no SDL/citro/std containers); picasso once per clean 3DS build; one `nnh-assetc mesh` step per model. Measured before/after (T0.2, T9.4). |
| IV Event-driven | ✅ | No events (FR-30); cameras, lights, stats are values and queries. |
| V No hidden costs | ✅ | No exceptions/RTTI/`virtual`; everything reserved at `init()` or load; recording O(1), cull O(n), sort O(n log n), no allocation per frame. |
| VI Spec + test first | ✅ | Every core and converter behaviour test-first on the fake platform; rasteriser by pixel tests; hardware by screenshots and probes. |
| VII Small files | ✅ | ~70 small files ([Files](#files)); `gfx3d.hpp` exceeds the 150-line *aim* (≈ 230 lines of mostly comments); no `.cpp` near 400. |
| VIII Documented API | ✅ | Three API pages, an export guide, example, rendering/sdl3/devkitpro pages (stage 10, 13). |
| IX Clean and legal | ✅ | Written from citro3d headers/examples (zlib), 3dbrew, the glTF spec, Giesen, Hecker, Eberly's paper; original models and fixtures. |
| X Safe by default | ✅ | Two new parsers (glTF in `nnh-assetc`, `.nnhmesh` in core), bounded, validated, fuzzed; misuse asserts, Release clamps; GPU state restored after each pass (no hang). |
| XI Opt-in | ✅ | 3D off by default (`max_draws_3d = 0`); math usable alone; code-built meshes need no assets. |

## Design

### Overview

```
game update()/draw()
  gfx3d::draw(Top, mesh, model, options) ─► record (88 B) + world sphere (16 B)      gfx3d/record.cpp
  gfx::draw / fill_rect / draw_text      ─► 2D draw list (unchanged)
end of frame: gfx_submit() → submit_screen(Top)
  DrawList::build (unchanged; TileSplit gains `layer`)
  g_gfx_3d_ops == null or no 3D draws on this screen ─► exactly today's submission (FR-18, AC-9)
  else gfx3d's screen pass (gfx3d/screen_pass.cpp):
     world split of the sorted items at world_layer (gfx/world_split.cpp)
     cull + order once (gfx3d/cull.cpp, order.cpp)
     for each eye (1 in 026a; 1–2 in 026b):
        [026b] platform::select_eye(eye)
        gfx_submit_slice(items below the world)              draw_2d / draw_tiles as today
        platform::begin_3d(pass for this eye) → draw_3d × n → end_3d()
        gfx_submit_slice(items from the world layer up)
  debug pass (per eye in 026b) → end_frame → present
  gfx_after_frame → ops->after_frame (probes, PC view restore, clear records)
```

### Public API

```cpp
// <nnh/math3d.hpp> — includes only <nnh/math.hpp> and <nnh/transform.hpp> (for SinCos/sin_cos). Declarations, plus
// constexpr single-operation helpers (+, −, × scalar: one IEEE operation each, so the game's flags can't fuse them).
namespace nnh {
struct Vec3 { float x = 0, y = 0, z = 0; /* +=, -=, *=, +, -, unary -, * float, / float, == */ };
struct Vec4 { float x = 0, y = 0, z = 0, w = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };          // identity by default
struct Mat4 {                                                // column-major, M × v (column vectors)
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    static constexpr Mat4 identity() { return {}; }
};
struct Transform3D { Vec3 position{}; Quat rotation{}; Vec3 scale{1, 1, 1}; }; // scale, then rotate, then move
struct Ray3 { Vec3 origin{}; Vec3 direction{0, 0, -1}; };

float dot(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
float length(Vec3 v);
float length_squared(Vec3 v);
Vec3 normalize(Vec3 v);                     // |v|² < 2⁻¹⁰⁰ → (0,0,0); asserts in Debug/Development
Quat quat_axis_angle(Vec3 axis, float radians);            // axis normalised inside
Quat quat_euler(float yaw, float pitch, float roll);       // = Y(yaw) × X(pitch) × Z(roll)
Quat look_rotation(Vec3 forward, Vec3 up = {0, 1, 0});     // −z of the result points along forward
Quat operator*(Quat a, Quat b);                             // a after b
Quat conjugate(Quat q);
Quat normalize(Quat q);
Vec3 rotate(Quat q, Vec3 v);                                // = to_mat4(q) applied to v (exact quarter turns)
Quat nlerp(Quat a, Quat b, float t);                        // shortest arc
Quat slerp(Quat a, Quat b, float t);                        // shortest arc, Eberly's polynomial, no libm
Mat4 to_mat4(Quat q);
Mat4 to_mat4(const Transform3D& t);
Mat4 operator*(const Mat4& a, const Mat4& b);
Mat4 transpose(const Mat4& m);
bool inverse(const Mat4& m, Mat4& out);                     // false (out untouched) when |det| < 2⁻¹⁰⁰ or not finite
bool inverse_affine(const Mat4& m, Mat4& out);              // rigid/scaled affine: 3×3 inverse + translation
Vec3 transform_point(const Mat4& m, Vec3 p);                // w = 1, divided by the result's w when it isn't 1
Vec3 transform_direction(const Mat4& m, Vec3 d);            // w = 0
Vec4 operator*(const Mat4& m, Vec4 v);
Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up);               // a view matrix (world → camera)
Mat4 perspective(float fov_y, float aspect, float near, float far);   // OpenGL clip space, depth −1..1
Mat4 orthographic(float height, float aspect, float near, float far);
bool is_finite(const Mat4& m);
}

// <nnh/mesh.hpp> — includes <nnh/math3d.hpp>, <nnh/gfx.hpp> (Color), <nnh/result.hpp>, <nnh/texture.hpp>.
namespace nnh {
enum class AlphaMode : std::uint8_t { Opaque, Cutout, Blend };
enum class TextureFilter : std::uint8_t { Nearest, Linear };
enum class TextureWrap : std::uint8_t { Repeat, Clamp, Mirror };
struct TextureId { int slot = -1; std::uint32_t generation = 0; };      // non-owning, value; {} = none
inline constexpr int kMaxShininess = 7;                                  // levels 0..7 = exponents 2..256
inline constexpr int kMaxSubmeshes = 32, kMaxMeshVertices = 65535, kMaxMeshTextures = 8;
struct Material3D {                                       // 28 bytes, trivially copyable
    Color base_color = colors::white;
    Color specular = colors::black;                       // rgb used; black = no highlight
    TextureId texture{};
    std::uint8_t shininess = 3;                           // level 0..7 (exponent 16 by default)
    AlphaMode alpha = AlphaMode::Opaque;
    std::uint8_t alpha_cutoff = 128;                      // Cutout: alpha < cutoff → discarded
    TextureFilter filter = TextureFilter::Nearest;
    TextureWrap wrap_u = TextureWrap::Repeat, wrap_v = TextureWrap::Repeat;
    bool lit = true, vertex_colors = true, cull_back = true;
};
TextureId texture_id(const Texture& texture);
struct Aabb3 { Vec3 min{}, max{}; };
struct Sphere { Vec3 center{}; float radius = 0; };
struct SubmeshDesc { int first_index = 0; int index_count = 0; Material3D material{}; };
struct MeshDesc {                                         // the game's arrays, read once by create_mesh
    const Vec3* positions = nullptr; int vertex_count = 0;
    const Vec3* normals = nullptr;                        // null → flat normals (vertices unwelded)
    const Vec2* uvs = nullptr;                            // null → no texture coordinates
    const Color* colors = nullptr;                        // null → white, no colour attribute
    const std::uint16_t* indices = nullptr; int index_count = 0;   // null → 0, 1, 2, …
    const SubmeshDesc* submeshes = nullptr; int submesh_count = 0; // null → one submesh, default material
};
class Mesh {                                              // move-only, RAII (frees linear + heap memory)
public:
    bool valid() const; int submesh_count() const; int vertex_count() const; int triangle_count() const;
    Aabb3 bounds() const; Sphere bounding_sphere() const; Material3D material(int submesh) const;
    void reset();
    /* slot_, generation_ (private), friend struct MeshAccess */
};
Result<Mesh> create_mesh(const MeshDesc& desc);           // quantised exactly as nnh-assetc does (AC-4)
Result<Mesh> make_cube(float size = 1, const Material3D& material = {});
Result<Mesh> make_plane(float width, float depth, int subdivisions = 1, const Material3D& material = {});
Result<Mesh> make_sphere(float radius, int rings = 12, int segments = 24, const Material3D& material = {});
Result<Mesh> load_mesh(const char* path);                 // "models/ship" → assets/models/ship.nnhmesh + textures
}

// <nnh/gfx3d.hpp> — includes <nnh/mesh.hpp>. No SDL, citro or standard container header.
namespace nnh {
enum class Eye : std::uint8_t { Left, Right };
enum class Projection : std::uint8_t { Perspective, Orthographic };
struct StereoSettings { bool enabled = true; float strength = 0; /* 0 = focal_distance / 20 */ float focal_distance = 5; };
struct Camera3D {
    Vec3 position{0, 0, 5}; Quat rotation{};              // looks down its −z, +y up
    Projection projection = Projection::Perspective;
    float fov_y = 1.04719758f;                            // radians (60°)
    float ortho_height = 10;                              // world units visible vertically (orthographic)
    float near = 0.1f, far = 100.0f;
    StereoSettings stereo{};
};
enum class LightKind : std::uint8_t { Directional, Point };
struct Light3D { LightKind kind = LightKind::Directional; Vec3 vector{-1, 1, 1}; /* direction towards the light, or position */
                 Color color = colors::white; float range = 10; };
struct Lighting { Color ambient{77, 77, 77, 255}; Light3D lights[4] = {Light3D{}}; int count = 1; };
struct DrawOptions3D { int submesh = -1; const Material3D* material = nullptr; Color tint = colors::white; };
struct ScreenPoint { Vec2 pixel{}; float depth = 0; bool in_front = false; bool in_view = false; };
struct Stats3D { int draws = 0, culled = 0, dropped_total = 0, peak_draws = 0, max_draws = 0;
                 int triangles[2] = {0, 0}; int eyes = 0; int meshes = 0; std::size_t mesh_bytes = 0; };
struct Probe { std::int16_t x = 0, y = 0; };

namespace gfx3d {
void point_at(Camera3D& camera, Vec3 target, Vec3 up = {0, 1, 0});
void set_camera(Screen screen, const Camera3D& camera);   Camera3D camera(Screen screen);
void set_lighting(Screen screen, const Lighting& lighting); Lighting lighting(Screen screen);
void set_world_layer(Screen screen, int layer);            int world_layer(Screen screen); // 0..16, default 8
void draw(Screen screen, const Mesh& mesh, const Mat4& model, const DrawOptions3D& options = {});
void draw(Screen screen, const Mesh& mesh, const Transform3D& transform, const DrawOptions3D& options = {});
ScreenPoint to_screen(Screen screen, Vec3 world_point);    // centre eye, the camera set now
Ray3 screen_ray(Screen screen, Vec2 pixel);
Stats3D stats();
void set_debug_draw(bool on);                              // Debug/Development; Release: nothing
bool request_probes(Screen screen, Eye eye, const Probe* points, int count); // ≤ 32; Debug/Development
int probe_results(Color* out, int capacity);               // the last request's colours, next frame
} // namespace gfx3d
}

// <nnh/system.hpp> (026b, appended):  float stereo_slider();  // 0..1; 0 without Capability::Stereo3D
// <nnh/app.hpp> GfxConfig (appended):   int max_draws_3d = 0;  // 3D draws per frame; 0 = 3D off; 1..4096
```

`Camera3D`'s `stereo.enabled = false` turns stereo off for that camera; orthographic cameras are always mono.

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| — | — | — | None (FR-30): cameras, lights, stats are values and queries. |

### Math (D44)

- Right-handed, +y up, camera down −z, radians, column vectors, column-major `Mat4::m`. Clip space as OpenGL
  (depth −1..1); the backends map it (3DS: depth to the PICA's range and the 90° screen rotation; PC: its own viewport).
- Only IEEE `+ − × ÷`, `__builtin_sqrtf` (`-fno-math-errno`: one correctly rounded instruction on x86-64 and VFP) and
  `sin_cos`; every `.cpp` of `src/math3d/` with `-ffp-contract=off`; a fixed operation order written out (no loops whose
  order a compiler may change; no `-ffast-math` anywhere).
- **Exact quarter turns.** `sin_cos(kQuarterTurn / 2)` gives s = c = 0.70710677 (not √½), so the usual
  `1 − 2(y² + z²)` leaves 6·10⁻⁸. `to_mat4(q)` instead computes `n = x² + y² + z² + w²` and every entry as a numerator
  over `n`: diagonal `(w² + x² − y² − z²)/n`, off-diagonal `2(xy − wz)/n`, …; for a quarter turn about an axis the
  numerators are exactly 0 or ±n, so entries are exactly 0/±1. `rotate(q, v)` uses that matrix. `quat_axis_angle`
  requires `sin_cos` to give identical bits for s and c at π/4 (T1.3 asserts it; if not, it snaps |s| = |c| when the
  angle is a quarter-turn multiple).
- **Tiny values.** The 3DS runs the VFP with flush-to-zero; the PC doesn't. Every normalise, inverse and division
  first tests the magnitude's exponent (< 2⁻¹⁰⁰ → the documented degenerate result), so no subnormal decides a
  result. Results are guaranteed identical for inputs whose magnitudes are 0 or within 2⁻⁶⁰…2⁶⁰ (documented).
- **slerp**: Eberly, "A Fast and Accurate Algorithm for Computing SLERP" (JGT 2011): the polynomial in cos θ with 8
  fixed coefficients; shortest arc by negating `b` when `dot < 0`; error < 2⁻¹⁸ per component (AC-3 measures).
- `perspective`: `tan(fov/2) = s/c` of `sin_cos(fov/2)`; `look_at` = `normalize`/`cross` in a fixed order;
  `look_rotation` → quaternion by Shepperd's method (branch on the largest of trace and diagonal; one `sqrt`).
- Accuracy (AC-3) against `double` references in tests: directions and unit quaternions ≤ 2⁻¹⁸ per component; points
  ≤ 2⁻¹⁶ × magnitude up to 4096 and chains of 8; measured in T1.6 and written into the API page.

### Meshes (D42)

**`.nnhmesh` version 1** (little-endian; layout fixed in `mesh/mesh_format.hpp` with `static_assert`s):

| Part | Bytes | Content |
|------|-------|---------|
| header | 112 | `"NNHM"`, version 1, header size, file size, CRC-32 of everything after the header, flags (bit 0 uv, bit 1 colour), vertex count (1..65535), index count (3..786 432, ×3), submesh count (1..32), texture count (0..8), stride (12/16/20), position scale[3] and offset[3], AABB min[3]/max[3], sphere[4], offsets of the four sections |
| submeshes | 48 each | first index, index count, min/max vertex, uv scale[2]/offset[2], material (base RGBA, specular RGB + shininess, texture index or −1, alpha mode, cutoff, filter, wrap u/v, flags lit/vertex colours/cull) |
| textures | 64 each | asset path without `.png`, NUL-padded (≤ 63 chars, `[A-Za-z0-9_./-]`, no `..`, no leading `/`) |
| vertices | count × stride | position int16 ×4 (x, y, z, 0) · normal int8 ×4 (x, y, z, 0) · [uv int16 ×2] · [colour u8 ×4] |
| indices | count × 2 (+2 pad) | u16 |

- Quantisation (pure `mesh/mesh_quantize.cpp`, shared with the converter): offset = AABB centre, scale per axis =
  `max(half extent, 2⁻²⁰)/32767`, `q = clamp(floor((p − offset)/scale + 0.5), ±32767)`; normals `floor(n × 127 + 0.5)`;
  uv per submesh over its range the same way; colours `floor(c × 255 + 0.5)`. Errors: positions ≤ half a step
  (extent/65534; 15 µm on a 1 m model, 7.8 mm on a 512-unit level: large levels split into meshes), normals ≤ 0.5°,
  uv ≤ range/65534. PC and 3DS decode with the same scale/offset: the decode is folded into the model-view matrix
  (`MV × T(offset) × S(scale)`), so the shader reads raw shorts.
- `mesh/mesh_build.cpp`: `MeshDesc` (floats) → bytes: bounds (AABB; sphere centre = AABB centre, radius = max distance,
  one `sqrt`), flat normals when missing (unwelded per triangle), layout, quantisation, CRC. `create_mesh` and
  `nnh-assetc mesh` both call it: **the same bytes** (AC-4).
- `mesh/mesh_validate.cpp` (shared with the converter's self-check): magic, version, sizes, aligned and ordered offsets
  inside the file, limits, stride vs flags, every index < vertex count, each submesh's range, `index_count % 3`,
  finite floats, scales > 0, enums in range, texture indices, paths, CRC. Never trusts the file; fuzzed.
- Loading (`mesh/load_mesh.cpp`): read `<path>.nnhmesh` through the assets module into a temporary heap buffer,
  validate, `platform::mesh_create` (vertices + indices to linear memory on 3DS; accounted `Linear`/`meshes` on every
  system), a small heap block (tag `meshes`) with submeshes, materials and owned textures (`load_texture(path)` each),
  free the buffer. The `Mesh` holds slot + generation into a 256-slot table; destroying it frees at once if not drawn
  this frame, else at the end of the frame (as textures).

### glTF converter (D42, FR-6/7)

`nnh-assetc mesh <in.gltf|.glb> <out.nnhmesh> --depfile <d> --assets-root <dir>` in `tools/assetc/src/gltf_*.cpp`,
reusing `json*.cpp` (bounded) and `png_size.cpp`:

- `gltf_glb.cpp` (container: header, JSON and BIN chunks, lengths), `gltf_document.cpp` (top-level arrays, limits,
  `extensionsRequired` against the known set {`KHR_materials_unlit`}), `gltf_accessors.cpp` (buffer views, accessors:
  component types, normalised, counts, strides, ranges inside buffers, no sparse), `gltf_nodes.cpp` (default scene,
  node tree depth ≤ 32, TRS or matrix → `Mat4` with math3d, baked; mirrored nodes flip winding; normals by the
  inverse-transpose), `gltf_primitives.cpp` (mode 4 only, indexed or not, attributes), `gltf_materials.cpp`
  (`baseColorFactor`, texture → image → PNG path rules, sampler filter/wrap, `alphaMode`, `alphaCutoff`, `doubleSided`
  → `cull_back = false`, unlit), `gltf_merge.cpp` (primitives sharing a material merged in first-appearance order;
  limits after merging) and `cli_mesh.cpp` (arguments, notes for ignored features, depfile with the `.bin` files and
  PNGs, `file: reason (at $.meshes[2].primitives[0])`).
- Limits (FR-7): `.gltf` ≤ 8 MiB, `.glb` ≤ 32 MiB, buffers total ≤ 32 MiB and ≤ 16, buffer views/accessors ≤ 1024
  each, meshes ≤ 256, primitives ≤ 256 in all, nodes ≤ 1024, depth ≤ 32, scenes ≤ 16, materials/textures/images/
  samplers ≤ 32 each, JSON depth ≤ 64, strings ≤ 4 KiB; after merging FR-4's limits and ≤ 8 textures.
- Ignored with one note line each (the model builds): skins, animations, morph targets, cameras, `KHR_lights_*`,
  `TEXCOORD_1+`, metallic/roughness, normal/occlusion/emissive maps, unknown *used* extensions.
- Refused: non-triangle modes, sparse accessors, unknown required extensions (Draco, meshopt, texture transform…),
  embedded images (`bufferView`) and data URIs (images or buffers), absolute or escaping paths, buffers not named by
  P25, non-PNG or rule-breaking textures (power of two 8–1024, read from the PNG header), malformed or out-of-range data.
- CMake (`cmake/nnh_assets.cmake`): `*.gltf`/`*.glb` in `ASSETS` → one step each → `<name>.nnhmesh` packed; P25's
  `.bin` files are not packed; `gltf`, `glb`, `nnhmesh` refused as data keys. The tool's mesh sources (and the shared
  core files) are compiled with `-ffp-contract=off` so Windows and Linux converters write the same bytes.

### Data and memory

| What | Size | Where | When |
|------|------|-------|------|
| Draw record | 88 B (model 3×4 48, material 28, mesh 4, tint 4, submesh 1, flags 1, call index 2) | Heap, tag `gfx3d` | × `max_draws_3d` at init |
| World sphere + order + sort key/scratch | 16 + 2 + 8 + 8 B | Heap | × `max_draws_3d` |
| Mesh table | 256 × 16 B = 4 KB | Heap | at init with 3D on |
| Lighting tables | 8 × 1 KB (D0) + 4 × 1 KB (distance, one per light) | 3DS: in `C3D_LightLut*`; PC: raster3d | at init |
| Shader | ≈ 1 KB `.shbin` | 3DS `.rodata` | linked only with 3D |
| Command buffer | + 640 B × `max_draws_3d` | 3DS linear | at `platform::init` |
| Top targets | left RGBA8 + D24S8 768 000 B (was 576 000), right 768 000 B (026b) | 3DS VRAM | only with 3D on |
| PC/fake buffers | per eye colour 384 000 B + depth 384 000 B, texel cache | PC heap (not accounted) | with 3D on |
| A mesh | vertices × 12–20 B + indices × 2 B (`Linear`, `meshes`); ~64 B + 48/submesh + textures (heap) | | at load |

Example: `max_draws_3d = 256` → 256 × 122 B ≈ 31 KB + 4 KB table ≈ 35 KB heap, +160 KB linear (command buffer),
+960 000 B VRAM (+576 000 B in 026a). AC-8's 12 000-triangle mesh (~6 100 vertices × 20 B + 36 000 × 2 B) ≈ 194 KB
linear. A 2D game: 0 bytes, its targets unchanged (AC-9).

### Shading on both systems (FR-10)

| Step | 3DS (PICA200) | PC/fake (raster3d) |
|------|---------------|--------------------|
| surface | vertex shader: `colour × (base × tint)/255` → primary colour; TexEnv 0: `TEXTURE0 × PRIMARY` (or `PRIMARY`) | `texel × vertex colour × base × tint` in float |
| lit diffuse + ambient | fragment lighting: light diffuse = colour, material diffuse/ambient = 1, `C3D_LightEnvAmbient` = ambient; point lights `C3D_LightDistAttn` with the shared table; TexEnv 1: `PREVIOUS × FRAGMENT_PRIMARY` | `surface.rgb × (ambient + Σ dᵢ·max(N·Lᵢ,0)·attnᵢ)` |
| specular | `D0` LUT on N·H (signed input, 0 below 0), table = shininess level (re-uploaded only when the level changes), material specular0 = specular colour; TexEnv 2: `PREVIOUS + FRAGMENT_SECONDARY` | `Σ sᵢ × specular × LUT(N·H)` with the same 256-entry table and its linear interpolation |
| unlit | lighting off, TexEnv 0 only | surface |
| cut-out | `C3D_AlphaTest(GEQUAL, cutoff)` | discard `a < cutoff` |
| blend | `C3D_AlphaBlend(src α, 1 − src α)`, depth write off | integer `(s·a + d·(255 − a) + 127)/255` |
| normal | shader: normal matrix × n, normalised, → quaternion output (`fragment_light` example) | interpolated view-space normal, normalised per pixel |

Three TexEnv stages of six; stages 3–5 pass through. Clamping on both: each channel to 0..255.

### Platform contract (D43): `engine/core/contract/nnh/platform/video3d.hpp` (new)

```cpp
namespace nnh::platform {
using MeshHandle = std::uintptr_t;
struct Video3DConfig { int max_draws = 0; };
bool video3d_init(const Video3DConfig& config);   // targets/depth, shader, LUTs; false + last_error()
void video3d_shutdown();
struct MeshUpload { const void* vertices; std::uint32_t vertex_count; std::uint8_t stride; std::uint8_t flags;
                    const std::uint16_t* indices; std::uint32_t index_count; };
struct MeshInfo { MeshHandle handle = 0; std::size_t bytes = 0; };
bool mesh_create(const MeshUpload& upload, MeshInfo& out);   // load time; copies (3DS: linear + cache flush)
void mesh_destroy(MeshHandle handle);                        // core calls it once no submitted frame uses it
struct Light3DGpu { std::uint8_t kind; float vector[3]; float color[3]; float inv_range; }; // view space
struct Pass3D { Eye eye; float projection[16]; float ambient[3]; Light3DGpu lights[4]; int light_count;
                bool below_2d; Color clear; };
struct Material3DGpu { TextureHandle texture; float base[4]; float specular[3]; std::uint8_t shininess, alpha,
                       cutoff, filter, wrap_u, wrap_v; bool lit, vertex_colors, cull_back; };
struct Draw3D { MeshHandle mesh; std::uint32_t first_index, index_count; float model_view[12]; float normal[9];
                float uv_scale[2], uv_offset[2]; Material3DGpu material; };
void begin_3d(Screen screen, const Pass3D& pass);  // between draw_2d calls of the screen
void draw_3d(const Draw3D& draw);                   // in order: opaque/cut-out then blended (core sorted)
void end_3d();                                      // restores the 2D state
// 026b:
void select_eye(Eye eye, Color top_clear);          // following Top draws go to that eye (cleared at first use)
void set_stereo(bool on);                           // this frame shows two eyes (3DS: gfxSet3D)
}
// remote.hpp (026b): bool remote_capture_right(ScreenPixels& out);
// system.hpp (026b):  float stereo_slider();
```

### Platform work

- **pc** (`engine/platforms/pc/src/`): `video3d_pc.cpp` (init: raster3d context, per-eye buffers, a streaming
  `SDL_Texture` per eye view, right target in 026b), `pass3d_pc.cpp` (begin: read back the eye target when `below_2d`
  else fill `clear`; draw: decode + rasterise; end: `SDL_UpdateTexture` + `SDL_RenderTexture` with
  `SDL_BLENDMODE_NONE` at 1:1), `mesh3d_pc.cpp` (copies), `texels3d_pc.cpp` (P16), `eyes_pc.cpp` (026b: eye selection by
  swapping `ctx.screens[0]`, right clear, the eye view composed after the debug pass (F8: copy, or anaglyph = left × red
  + right × cyan with SDL colour mod and `SDL_BLENDMODE_ADD`), restored in `after_frame`); `window_pc.cpp` /
  `sim_settings.cpp` (026b: F6/F7/F8, title notice, `window.ini` keys `slider`, `eye`); `system_pc.cpp`
  (`Stereo3D`, `stereo_slider()`); `remote_capture_right_pc.cpp`.
- **3ds** (`engine/platforms/3ds/`): `shaders/mesh3d.v.pica` (inputs v0 position, v1 normal, v2 uv, v3 colour;
  uniforms projection, model-view 3×4, normal 3×3, uv scale/offset, colour scale; outputs position, normal quaternion,
  view vector, texcoord0, colour; ≈ 30 instructions), CMake `ctr_add_shader_library(nnh_mesh3d_shader
  shaders/mesh3d.v.pica)` (sources as unparsed arguments: the function rejects `SOURCES`) +
  `dkp_add_embedded_binary_library(nnh_mesh3d_shbin nnh_mesh3d_shader)` linked into `platform_3ds` (pulled only by
  `video3d_3ds.cpp`); `video3d_3ds.cpp` (init: replace the top-left target by `C3D_RenderTargetCreate(240, 400,
  GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8)` + `C3D_RenderTargetSetOutput(GFX_TOP, GFX_LEFT, flags)`, the program, attribute
  layouts for 12/16/20-byte strides with `AttrInfo_AddFixed` for missing attributes, LUTs; after every
  `C2D_Prepare()` `C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR)`), `pass3d_3ds.cpp` (begin: `C2D_Flush()`, bind
  program/attr, `C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL)`, light env, projection = core's × the fixed tilt/depth
  matrix; draw: uniforms, material (cached: texture, LUT, TexEnv, cull, alpha test/blend only when they change),
  `C3D_TexSetFilter/Wrap` + `C3D_TexBind`, `BufInfo` per mesh, `C3D_DrawElements(GPU_TRIANGLES, n, C3D_UNSIGNED_SHORT,
  ptr)`; end: restore touched texture params, `C3D_LightEnvBind(nullptr)`, alpha test off, default blend, cull none,
  `C2D_Prepare()`, depth off), `lights3d_3ds.cpp`, `mesh3d_3ds.cpp` (`linearAlloc`, `GSPGPU_FlushDataCache`),
  `eyes_3ds.cpp` (026b: right target, swap `targets[0]`, `C2D_TargetClear` at first right use — it flushes and splits
  the frame first, so it is ordered — `gfxSet3D` only on change), `slider_3ds.cpp` (`osGet3DSliderState`),
  `remote_capture_right_3ds.cpp`; `app_3ds.cpp`: command buffer size (P22).
- **fake** (`engine/platforms/fake/`): `video3d_fake.cpp` records every `begin_3d`/`draw_3d`/`end_3d`, `select_eye`,
  `set_stereo` and mesh call (`fake::pass3d_log()`), optionally rasterises with raster3d (`fake::enable_pixels_3d(true)`,
  `fake::pixel_3d(eye, x, y)`), keeps RGBA copies of textures made by `texture_create_rgba` for that; `fake::
  set_stereo_slider(v)` (026b), `remote_capture_right` synthetic.
- **shared** `engine/platforms/shared/raster3d/` (P17, D41): `raster3d.hpp` (context, buffers, texture view, draw
  input), `vertex_stage.cpp` (decode, MV, P, view-space normal/view vector), `clip.cpp` (homogeneous near/far + guard
  band, Sutherland–Hodgman, ≤ 9 vertices), `setup.cpp` (fixed-point 28.4 edges, top-left bias, culling by signed area),
  `raster.cpp` (bounding-box walk with incremental edges, depth test, perspective-correct barycentrics), `shade.cpp`
  (FR-10, LUTs), `sample.cpp` (nearest/bilinear, repeat/clamp/mirror), `blend.cpp`, `lut.cpp` (table generator shared
  with the 3DS backend through a pure core file `gfx3d/light_tables.cpp`).

### Core work (gfx side, always linked — kept to hooks)

- `draw_list.hpp/.cpp`: `TileSplit` gains `std::uint8_t layer` (set in `build()` from the command's layer).
- `gfx/world_split.hpp/.cpp` (pure): first item and first split at or above the world layer.
- `gfx_frame.cpp`: the loop of `submit_screen` becomes `detail::gfx_submit_slice(screen, camera, result, item range,
  split range)` (same calls in the same order); `submit_screen` asks `g_gfx_3d_ops->submit_screen(screen, result)`
  first when the pointer is set; `gfx_init`/`gfx_shutdown` call the hook's init/shutdown when `max_draws_3d > 0`.
- `gfx/gfx3d_hook.hpp/.cpp`: `struct Gfx3DOps { init, shutdown, submit_screen, eyes_this_frame, select_eye,
  after_debug, after_frame, overlay_line }` and `const Gfx3DOps* g_gfx_3d_ops = nullptr`.
- `debug/debug_pass.cpp` (026b): the top screen's debug items are sent once per eye through the hook.
- `debug/overlay.cpp`: one line from `overlay_line` when the hook is set.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/math3d.hpp` | new | FR-1 values and declarations |
| `engine/core/src/math3d/vec3.cpp`, `quat.cpp`, `quat_basis.cpp`, `quat_interp.cpp`, `mat4.cpp`, `mat4_inverse.cpp`, `projection.cpp`, `transform3d.cpp`, `finite3d.cpp`, `math3d_internal.hpp` (exponent tests, `sqrt`) | new | FR-1–3 (`-ffp-contract=off`; `-fno-math-errno` where `sqrt`) |
| `engine/core/include/nnh/mesh.hpp` | new | meshes, materials, `TextureId` |
| `engine/core/src/mesh/mesh_format.hpp`, `mesh_layout.cpp`, `mesh_validate.cpp`, `mesh_quantize.cpp`, `mesh_bounds.cpp`, `flat_normals.cpp`, `mesh_build.cpp` | new (pure, shared with assetc) | D42 format, FR-4/5/7 |
| `engine/core/src/mesh/primitives.cpp`, `mesh_table.cpp`, `mesh_table.hpp`, `create_mesh.cpp`, `load_mesh.cpp`, `mesh.cpp` (handle), `mesh_internal.hpp` | new | FR-5, FR-8 |
| `engine/core/include/nnh/gfx3d.hpp` | new | FR-9–20, 28 public API |
| `engine/core/src/gfx3d/gfx3d_state.hpp`, `gfx3d_ops.cpp` (registrar), `gfx3d_init.cpp`, `record.cpp`, `material_check.cpp`, `camera3d.cpp`, `eye_setup.cpp`, `frustum.cpp`, `cull.cpp`, `order.cpp`, `lighting.cpp`, `light_tables.cpp`, `screen_pass.cpp`, `submit3d.cpp`, `screen_project.cpp`, `stats3d.cpp`, `debug_draw3d.cpp`, `probe.cpp` | new | recording, pass, culling, order, lights, eyes, queries, probes |
| `engine/core/src/gfx/world_split.hpp/.cpp`, `gfx3d_hook.hpp/.cpp` | new | split, hook |
| `engine/core/src/gfx/draw_list.hpp/.cpp`, `gfx_frame.cpp`, `gfx_internal.hpp` | changed | `TileSplit::layer`, slice function, hook calls |
| `engine/core/src/debug/debug_pass.cpp`, `overlay.cpp` | changed (026b / 026a) | per-eye debug pass; overlay line |
| `engine/core/include/nnh/app.hpp` | changed | `GfxConfig::max_draws_3d` |
| `engine/core/include/nnh/system.hpp`, `src/system/stereo_slider.cpp` | changed/new (026b) | FR-16 |
| `engine/core/contract/nnh/platform/video3d.hpp` | new | D43 contract |
| `engine/core/contract/nnh/platform/system.hpp`, `remote.hpp` | changed (026b) | slider, right-eye capture |
| `engine/core/src/remote/remote_wire.hpp/.cpp`, `remote_session.cpp`, `remote_screenshot.cpp`, `remote_slider.cpp` | changed/new (026b) | `Slider` message, screen 2 |
| `engine/core/CMakeLists.txt` | changed | three source blocks (math3d, mesh, gfx3d) after the hierarchy block |
| `engine/platforms/shared/raster3d/*` + `CMakeLists.txt` | new | D41 rasteriser |
| `engine/platforms/pc/src/video3d_pc.cpp`, `pass3d_pc.cpp`, `mesh3d_pc.cpp`, `texels3d_pc.cpp`, `eyes_pc.cpp`, `remote_capture_right_pc.cpp` | new | PC backend |
| `engine/platforms/pc/src/window_pc.cpp`, `sim_settings.*`, `context_pc.hpp`, `system_pc.cpp` | changed (026b) | slider, eye view, capability |
| `engine/platforms/3ds/shaders/mesh3d.v.pica`, `src/video3d_3ds.cpp`, `pass3d_3ds.cpp`, `lights3d_3ds.cpp`, `mesh3d_3ds.cpp`, `tilt3d_3ds.cpp`, `eyes_3ds.cpp`, `slider_3ds.cpp`, `remote_capture_right_3ds.cpp`, `CMakeLists.txt` | new/changed | 3DS backend |
| `engine/platforms/3ds/src/app_3ds.cpp` | changed | command buffer size |
| `engine/platforms/fake/src/video3d_fake.cpp`, `include/nnh/platform_fake.hpp` | new/changed | records, pixels, slider |
| `tools/assetc/src/gltf_*.cpp/.hpp`, `cli_mesh.cpp`, `main.cpp`/`cli.cpp` (dispatch), `CMakeLists.txt` | new/changed | converter |
| `tools/assetc/tests/test_gltf_*.cpp`, `test_cli_mesh.cpp`, `libfuzzer_assetc_gltf.cpp`, `fuzz_assetc_gltf.cpp` | new | AC-4 |
| `tests/data/gltf/make_fixtures.py`, `*.gltf`, `*.bin`, `*.glb`, `*.png`, `bad_*.gltf` + `bad_*.txt` | new | fixtures |
| `cmake/nnh_assets.cmake` | changed | mesh step, P25 routing |
| `tools/nnh/src/remote/script*.{hpp,cpp}`, `game_session.cpp`, `screen_image.*` | changed (026b) | `slider`, `screenshot NAME eyes` |
| `tests/math3d/`, `tests/mesh/`, `tests/gfx3d/`, `tests/raster3d/`, `tests/gfx/test_world_split.cpp`, `tests/platform_conformance/test_video3d.cpp`, `tests/fuzz/` (mesh reader), `tests/models_example/` | new | AC-1–5 |
| `examples/models/` (`main.cpp`, `src/*`, `art/make_models.py`, `assets/`, `tests/check.nnhs`, `tests/bench.nnhs`, `tests/stereo.nnhs`) | new | AC-6–8 |
| docs (stage 10/13) | new/changed | AC-10 |

### Example `examples/models` (AC-6–8)

`MODELS_DEMO=showcase|check|bench`. Files: `main.cpp`, `src/showcase_scene.*`, `src/check_scene.*`,
`src/bench_scene.*`, `src/self_check.*` (AC-2 script; shared with `tests/models_example/`), `src/probe_scenes.*`
(fixed scenes + expected colours from a plain-float FR-10 reference, `src/shading_ref.*`), `src/hud.*`. Showcase: the
generated `ship` model (textured, lit, spinning), a floor plane, a code cube and sphere, a cut-out fence, a blended
glass pane, a directional + circling point light, a 2D sky gradient (layers 0–7), HUD and `to_screen` name tag
(layer ≥ 8), Circle Pad orbit, L/R zoom, X perspective/orthographic, bottom-screen touch buttons (texture, lighting,
point light, culling, debug draw, tint flash), slider value. Logs `tag: <x>,<y> depth <d>` on each button change.

## Cost estimates (Old 3DS, AC-8)

Per the lessons (instruction-count estimates 1.3–2× low; 16 KB L1D, no L2, no prefetcher, ~110 cycles a miss; VFPv2
scalar, `vdiv`/`vsqrt` ≈ 15 cycles):

| Workload | Estimate (with the ×1.3–2 margin) | Target | Verdict |
|----------|-----------------------------------|--------|---------|
| Record one draw (validate 12 floats by bits, mesh slot read ~1 miss, sphere: 3×3 + 3 column lengths + `sqrt`, 88 B write ~3 lines) | 1.2–2.5 µs | — | |
| Cull one draw at submit (16-B sphere array, 6 planes) | 0.3–0.6 µs | — | |
| Submit one draw per eye (MV 3×4 × 4×4 + quantisation fold + cofactor normal ≈ 200 flops; citro3d uniforms ≈ 40 words; `C3D_DrawElements` + dirty checks) | 4–9 µs | — | |
| **CPU alone**: 512 record + cull + sort 128 blended (merge sort, 8-B keys) | **0.8–1.6 ms** | < 1.0 ms | **at risk** |
| **Draws**: 256 cubes, mono (record + cull + submit) + HUD | **1.4–3.0 ms** CPU (+ citro2d HUD ≈ 0.5 ms) | < 4 ms | likely |
| **Draws**: stereo | **2.5–5.4 ms** | < 7 ms | likely |
| **Throughput**: 12 000 triangles textured + 1 light, half the screen | GPU: vertex shader 0.2–1.0 ms (with/without a post-transform cache), setup 1–4 ms, fill 0.5–1.5 ms | 60 Hz mono | likely; unknown setup rate (measured) |
| 6 000 triangles stereo | about the mono cost | 60 Hz | likely |
| **Fill**: 3 full-screen blended planes (288 000 px) over an opaque scene | 1.5–4 ms GPU | 60 Hz | likely |
| Shininess change (LUT upload 256 words) | ≈ 3–5 µs each | — | cached per level |
| **2D unchanged** (tile gate) | 0 (2D-only build: no 3D code) | pass | — |
| Simulator showcase 3D pass (pc-dev): rasterise ≈ 70 000 shaded px at 40–75 ns + read-back ≈ 1 ms + upload 0.3 ms | 4–7 ms | < 4 ms | **at risk** → optimise spans first |

If a target is still missed after optimisation (cache-friendly record layout, culling at record time when the camera
is already final, material/state caching), the budget is set from the first measurement with margin and written in
the spec and docs (the maintainer's policy).

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/math3d/test_vec3.cpp`, `test_quat.cpp`, `test_quat_interp.cpp`, `test_mat4.cpp`, `test_mat4_inverse.cpp`, `test_projection.cpp`, `test_transform3d.cpp` |
| FR-2 | `tests/math3d/test_conventions.cpp` (handedness: x × y = z; `look_at` down −z; column vectors; storage order) |
| FR-3 | `test_math3d_exact.cpp` (identity, quarter turns, translations), `test_math3d_accuracy.cpp` (double reference, AC-3), `tests/gfx3d/test_golden_3d.cpp` (AC-2, 50 000 ops; same on GCC 13/14, clang 18, MinGW, console self-check) |
| FR-4 | `tests/mesh/test_mesh_layout.cpp`, `test_mesh_quantize.cpp` (round trips, error bounds), `test_mesh_limits.cpp` |
| FR-5 | `tests/mesh/test_create_mesh.cpp`, `test_primitives.cpp` (counts, normals, bounds, bytes stable) |
| FR-6 | `tools/assetc/tests/test_gltf_*.cpp` per fixture; `test_cli_mesh.cpp` (each `bad_*` → its message); same bytes as `create_mesh` |
| FR-7 | `tests/mesh/test_mesh_validate.cpp` (corruptions), `tests/fuzz/` mini-fuzz + `libfuzzer_mesh_reader.cpp`, `libfuzzer_assetc_gltf.cpp` (10 min each on the VM) |
| FR-8 | `tests/mesh/test_load_mesh.cpp` (fake assets: textures loaded, accounting `Linear`/`meshes`, reset, stale draws skipped) |
| FR-9 | `tests/gfx3d/test_material.cpp` (defaults, overrides, tint, invalid values) |
| FR-10 | `tests/raster3d/test_shade.cpp` (±1 against the float formula); console probes (AC-7) |
| FR-11 | `tests/gfx3d/test_lighting.cpp` (defaults, 4 max, view-space conversion, falloff table) |
| FR-12 | `tests/gfx3d/test_camera3d.cpp` (projection, aspect, bottom screen refused) |
| FR-13 | `tests/gfx3d/test_screen_project.cpp` (round trips, in_front/in_view, ortho) |
| FR-14 | `tests/gfx3d/test_eyes.cpp` (026b: off-axis, focal plane at screen depth, slider 0 → 1 eye, ortho → 1 eye, stereo off) |
| FR-15 | `tests/gfx3d/test_screen_pass_stereo.cpp` (026b: identical 2D item lists and debug pass per eye on fake) |
| FR-16 | `tests/system/test_stereo_slider.cpp`, `tests/remote/test_remote_wire.cpp` (Slider), `tools/nnh/tests/test_script_parser.cpp`; simulator keys by screenshot |
| FR-17 | `tests/gfx3d/test_record.cpp` (capacity, drops, outside a frame, 3D off) |
| FR-18 | `tests/gfx/test_world_split.cpp`, `tests/gfx3d/test_screen_pass.cpp` (call sequence on fake; 2D-only frames byte-identical to the old sequence) |
| FR-19 | `tests/gfx3d/test_order.cpp` (call order, back to front, ties) |
| FR-20 | `tests/gfx3d/test_cull.cpp` (inside, outside, straddling, union), `tests/raster3d/test_clip.cpp` |
| FR-21 | golden (AC-2) includes order, culling, stats; console self-check |
| FR-22 | `tests/raster3d/*` (fill rule, depth, perspective, clip, cut-out, blend, cull, filters, wraps), `tests/platform_conformance/test_video3d.cpp`, simulator screenshots |
| FR-23 | golden PNG hashes of the showcase on Windows and Linux (3 renderers each) — `scripts/check_png_hash.sh` |
| FR-24 | example check mode probes, console run |
| FR-25 | `tests/gfx3d/*` read `fake::pass3d_log()`; pixel tests with `fake::enable_pixels_3d` |
| FR-26 | `tests/gfx3d/test_init3d.cpp` (0 = nothing reserved, memory report), 3DS memory/VRAM report |
| FR-27 | `tests/gfx3d/test_no_alloc.cpp` (alloc counter over 100 frames) |
| FR-28 | `tests/gfx3d/test_stats3d.cpp`, `test_debug_draw3d.cpp` |
| FR-29 | `tests/gfx3d/test_misuse3d.cpp` (death tests per case; Release behaviour by `NNH_ASSERT` off build in the VM) |
| FR-30 | review: no `publish` in the modules (grep in T9.3) |
| FR-31 | `scripts/check_no_3d_symbols.sh` on `hello` (3DS `.map`, PC `nm`); size table; build-time table |

Mutation (T9.2): hand mutations of the quarter-turn path, the tiny-value tests, the sort tie rule, the top-left bias,
the culling union and the quantisation rounding must each fail a test.

## Risks and alternatives

- **AC-9's "same size"** can't be literally 0 bytes (hook null checks, PC keys): P14 budget, measured, recorded.
- **CPU-alone and simulator targets at risk** (table above): optimise first, then the measurement policy.
- **PICA details found only on hardware**: the tilt/depth matrix (T7.4 compares with `Mtx_PerspTilt` on the console),
  `D0` signed input, attribute alignment for the 12/16/20 strides, citro2d state after `C2D_Prepare` (alpha test/blend
  not reset). Mitigation: stage 7 ends with a console smoke run (flat cube, then lit) before the example.
- **Post-transform cache unknown**: if 12 000 triangles miss 60 Hz, the converter can reorder indices for locality (a
  follow-up) or the target is set from measurement.
- **PC read-back cost** with the GPU renderer: if > 2 ms, fall back to premultiplied composition for frames whose 3D has
  no blended pixel over 2D-only pixels (golden scenes chosen so) — decided in T6.5 with numbers.
- **Determinism across compilers** of the rasteriser (vectorisation can't reorder without fast-math; int conversions
  clamped first): goldens run on the VM in stage 9; a mismatch is a bug, not a tolerance.
- **Merge conflicts** with 023/024 in `gfx_frame.cpp` and `app.hpp` (see below).
- **world_layer 8 vs text layer 0** surprises users (P13): documented in the header, API page and example.
- Alternatives kept for later behind the same contract: an SDL GPU path in the simulator (follow-up 10), shared depth
  between eyes (−384 KB VRAM), index reordering in the converter.

## Merge notes

Implementation starts from `main` **after spec 022 is complete there**, in a worktree branch `spec-026`. Rebase on
`main` at the start of stages 4, 6, 11 and before the final merge. Every change to a shared file is an insertion at a
distinct anchor:

| Shared file | 026's change (anchor) | Other specs | Resolution |
|-------------|----------------------|-------------|------------|
| `engine/core/CMakeLists.txt` | three blocks `_nnh_math3d_sources`, `_nnh_mesh_sources`, `_nnh_gfx3d_sources` **after the hierarchy block** (`_nnh_hierarchy_sources`'s `endif()`), before `add_library(nnh::core ALIAS` | 023/024/025 append their blocks at the same place | keep every block; order irrelevant |
| `engine/core/include/nnh/app.hpp` | `int max_draws_3d = 0;` as the **last** `GfxConfig` member | 024 adds `max_particles` after `max_tiles` | keep both; 026's stays last |
| `engine/core/src/gfx/gfx_frame.cpp` | extract `gfx_submit_slice()` from `submit_screen`'s loop; hook calls in `gfx_init` (after `tiles_init` succeeds), `gfx_shutdown` (before `free_all`), `submit_screen` (after `build`) | 023: `NNH_PROFILE_SCOPE("gfx submit")` at the top of `gfx_submit()`; 024: dispatch inside the split loop, `free_all`, `gfx_init`, after `tiles.clear()` | if 024 is on `main` first, its split dispatch (`< kMaxTileBatches` → tiles, else particle ops) moves **unchanged** into `gfx_submit_slice`; 023's hunk untouched; run 024's and 026's submit tests after the merge |
| `engine/core/src/gfx/draw_list.hpp/.cpp` | `TileSplit::layer` + one assignment in `build()` | 024 moves sprite placement to `sprite_place.hpp` | independent hunks |
| `engine/core/src/debug/overlay.cpp` | one null-checked line after the gfx line | 024 adds a particles line | keep both |
| `engine/core/src/debug/debug_pass.cpp` (026b) | eye loop around the top screen's `draw_2d` | — | — |
| `engine/platforms/3ds/src/app_3ds.cpp` | command buffer size before `C3D_Init` | 024 changes `max_objects` | distinct lines |
| `cmake/nnh_assets.cmake` | `.gltf`/`.glb` step **before the generic rules**, next to the `.ase.json` step; `.bin` exclusion | 024 may add engine schema sources (`cmake/nnh_data.cmake`) | independent |
| `tools/assetc/src/cli.cpp`/`main.cpp` | `mesh` subcommand row | — | — |
| `engine/core/contract/nnh/platform/system.hpp`, `remote.hpp`, `remote_wire.*` (026b) | appended declarations, message type 7, screen 2 | — | — |
| `docs/architecture/decisions.md` | D41–D44 rows after D40, one change-log line (done with this plan) | — | — |
| `docs/architecture/rendering.md`, `docs/tech/sdl3.md`, `devkitpro-3ds.md` | stage 10 (planned, not edited now) | 024 edits rendering.md (particle batches) | edit the "3D later" sections only |
| `specs/README.md`, `docs/ROADMAP.md`, `CLAUDE.md` | status and layout at the end (T13.9) | all specs | rebase, keep every row |

025 (pathfinding) shares only `CMakeLists.txt` blocks and docs indexes.

## Constitution Check (after design)

Unchanged from the first check. Notes: III — the picasso step and the per-model step are new build work, both
measured (T0.2/T9.4); VII — `gfx3d.hpp` passes the 150-line aim (comments), no `.cpp` near 400; XI — 3D is off by
default and a 2D game links no 3D code (P14's null checks are the only cost, recorded against AC-9). No exception needs
the maintainer's approval beyond the decisions above.

## Stage results and refinements

### Stage 0 results (T0.1–T0.2)

- T0.1: spec 022 is on `main` with the names the plan expected: `nnh::sin_cos` and the quarter-turn constants in
  `<nnh/transform.hpp>`, `detail::sin_cos_inline` in `src/transform/sin_cos.hpp`, `detail::sin_quarter_inline` in
  `src/core/det_sine.hpp`; `nnh_tests transform` green.
- T0.2 baselines (Windows, WinLibs GCC, devkitARM; builds of other worktrees ran at the same time, so times are noisy):

  | Measure | Value |
  |---------|-------|
  | `hello` 3DS Release `.text` / `.data` / `.bss` | 201 124 / 28 352 / 106 680 B |
  | `hello.3dsx` (3DS Release) | 246 512 B |
  | `hello.exe` (PC Release) / its `.text` | 278 856 B / 130 668 B |
  | edit `examples/hello/main.cpp` → rebuilt (pc-debug / 3ds-release) | 1.7 s / 3.0 s |
  | pc-release, 3ds-release, 3ds-dev builds | 0 warnings |

- `scripts/example_shots.sh [preset] [frames] [out]` (new) runs every example headless (`SDL_VIDEO_DRIVER=offscreen`,
  software renderer, `NNH_SIM_RENDERER=sdl`) for 30 frames and prints each screenshot's SHA-256; the pc-debug list is
  saved in `build/baseline-026/shots.txt` and compared at every stage end (AC-9: 2D frames byte-identical).

### Stage 1 results and refinements (T1.1–T1.7)

- Files: `is_finite(Mat4)` lives in `mat4.cpp` (no `finite3d.cpp`); the look basis shared by `look_rotation` and
  `look_at` and the eighth-turn snap are in `quat_basis.cpp` (`math3d_internal.hpp` declares them).
- **Eighth-turn snap (P4)**: rather than testing the bits of `sin_cos(kQuarterTurn/2)`, `quat_axis_angle` and
  `perspective` pass every half angle through `detail::snap_eighth`: when |sin| and |cos| differ by at most 2⁻²⁰ both get
  the larger magnitude with their own signs. Quarter, half and three-quarter turns (and their products) map axes to axes
  exactly; `perspective(kQuarterTurn, …)` has a focal length of exactly 1.
- **slerp**: Eberly's 8-term polynomial measured a worst error of 1.9·10⁻⁵ against `double` over the whole range
  (x = cos θ ∈ [0, 1]), above FR-3's 2⁻¹⁸. The same recurrence with **16 terms** and the last pair scaled by
  1 + μ = 1.9092 (fitted by search) measures **1.2·10⁻⁷** (float rounding); cost 16 multiply-adds per coefficient.
- `normalize` is overloaded for `Vec3` and `Quat` (P3), so a braced argument is ambiguous: write
  `normalize(Vec3{…})` (documented in the API page).
- Degenerate look directions: `forward` parallel to `up` uses the up (0, 0, ∓1), so a camera looking straight down has
  −z (north) at the top of the screen and +x on the right.
- Measured accuracy (AC-3, `test_math3d_accuracy.cpp`, `test_quat_interp.cpp`): unit quaternions from axis-angle
  8.7·10⁻⁷, rotated directions 1.4·10⁻⁶ (bound 2⁻¹⁸ = 3.8·10⁻⁶); points through chains of 8 transforms with coordinates up
  to 4096: 3.5·10⁻⁶ of the magnitude (bound 2⁻¹⁶ = 1.5·10⁻⁵); slerp 1.2·10⁻⁷.
- T1.7: the golden script lives in the example from the start (`examples/models/self_check.{hpp,cpp}`, compiled into
  `nnh_tests` with `-ffp-contract=off`), so the console and the tests run the same code. Inputs are FNV-1a hashes scaled
  by powers of two (exact); stage 1's math-only golden after 50 000 operations: `436E15A9` (checkpoints `376AB001`,
  `804B29F2`, `C4D52290`, `48556FCB`). It changes once, in T4.10, when frustum tests, orders and `to_screen` join.

### Stage 2 results and refinements (T2.1–T2.7)

- **Format**: the header packs flags, submesh count, texture count and stride in one word to stay at 112 bytes; a
  submesh record is 48 bytes (16-byte material record + one reserved word). The structs are the bytes (both systems are
  little-endian); the reader copies with `memcpy` (no alignment assumed). The index section is padded to 4 bytes.
- **Texture coordinates**: one uv range per mesh (stored in every submesh record): a vertex shared by two submeshes
  would otherwise need two quantisations. The per-submesh fields stay in the format for later.
- **Bounds**: the box and the sphere are computed from the *decoded* positions (what the GPU draws); the sphere's centre
  is the box centre (the position offset).
- Files: `mesh_measure.cpp` (validation and sizing) beside `mesh_build.cpp` (writing); flat normals live in
  `mesh_quantize.cpp` (`face_normal`); `mesh_record.cpp` (records, upload, accounting) and `mesh_table.cpp`; one test
  file for layout/quantisation/flat normals/build/limits (`test_mesh_build.cpp`).
- **Hook moved forward (part of T4.1 done here)**: `GfxConfig::max_draws_3d`, `gfx/gfx3d_hook.*` and the registrar
  `gfx3d/gfx3d_ops.cpp` land in stage 2 because meshes need 3D on (P6): `mesh_table_ready()` calls `gfx3d_link()`, so a
  game that makes a mesh links the registrar. 3D init failing (`video3d_init`) fails `nnh::init()`.
- **Deferred frees mirror textures**: a mesh released inside a frame stays usable by that frame's draws and is freed in
  the frame's `after_frame` (2D textures behave the same way, so "a draw recorded before is skipped" reads "is drawn
  that frame, then the mesh is gone").
- The fake platform logs `[` and `]` for `begin_3d`/`end_3d` in `State::calls` (2D-only frames unchanged) and keeps
  meshes' bytes (`mesh_data`) for the pixel tests; the PC and 3DS backends are stubs returning false until stages 6–7.
- Measured: `hello` 3DS Release `.text` +72 B (201 196), `.3dsx` +72 B, PC Release `.exe` +489 B — the hook pointer and
  its null checks (P14 budget: 128 B / 1 KB). Every example's screenshot unchanged (`audio` and `foundation` show live
  timings and differ run to run, so they are left out of the comparison). Primitive bytes: cube CRC `9A45BD39`,
  `make_sphere(0.5, 8, 12)` CRC `BBF6FDA6`.

### Stage 3 results and refinements (T3.1–T3.8)

- Files: the .glb container is read in `gltf_document.cpp` (no `gltf_glb.cpp`); primitives and the merge share
  `gltf_primitives.cpp` (no `gltf_merge.cpp`); the orchestration is `convert_mesh.cpp` + a `mesh` row in `cli.cpp` (no
  `cli_mesh.cpp`). The CLI takes `--assets` like `map` and `sprite` (not `--assets-root`) and prints one
  `<model>: note: ignored: …` line per ignored feature on standard output.
- **Baking in double, not math3d**: node matrices, the cofactor normal matrix and positions are computed in `double`
  (IEEE + − × ÷ and sqrt: the same bits on every host), so the host tool doesn't link core's math (and its asserts).
  The baked floats then go through the engine's own builder (`mesh_build.cpp`, compiled into the tool with
  `-ffp-contract=off -fno-math-errno`): `test_cli_mesh.cpp` checks the converted cube's bytes equal create_mesh's builder
  output for the same arrays (AC-4).
- Mixed normals: when every primitive lacks `NORMAL` the builder makes flat normals (as create_mesh would); when only
  some lack it, those are unwelded in the converter with the builder's `face_normal`. Missing `TEXCOORD_0`/`COLOR_0`
  in some primitives become (0, 0) and white.
- Defaults: no sampler → linear filtering (as Blender shows it); `alphaCutoff` 0.5 → 128; `doubleSided` → no culling;
  glTF factors are converted to bytes as given (no sRGB conversion).
- The JSON reader's depth limit stays 32 (glTF documents nest ≤ 6); the other limits as planned.
- Fixtures: `tests/data/gltf/make_fixtures.py` (7 good models incl. `cube.glb`, 12 `bad_*` with expected messages);
  "more than 65535 vertices" and the notes are tested from memory (no 800 KB fixture). Mini-fuzz: 599 of 8 000 mutated
  models convert and validate; libFuzzer target `fuzz_assetc_gltf`.
- CMake: `.gltf`/`.glb` → `.nnhmesh` step with a depfile (model, buffers, PNGs), P25 buffers not packed, two sources
  becoming the same asset refused; `gltf`, `glb`, `nnhmesh` are reserved data keys. The `ninja -d explain` check of
  "editing one model reconverts only it" runs with the example's two models in stage 8.

### Stage 4 results and refinements (T4.1–T4.11)

- **API deviations (recorded for the maintainer)**: `Camera3D::near_plane`/`far_plane` instead of `near`/`far`
  (`<windows.h>` defines `near` and `far` as empty macros: a Windows game including it would break the struct);
  pure overloads `gfx3d::to_screen(const Camera3D&, Vec3)` and `gfx3d::screen_ray(const Camera3D&, Vec2)` (the top
  screen's 400×240) so the self-check and tools use them without an engine; `request_probes`/`probe_results` exist but
  answer nothing until stage 7.
- Record = 92 bytes (model rows 48, material 28, mesh 6, submesh 1, flags 1, tint 4, call 2, padding 2); with the
  world sphere, view depth, two order entries, sort scratch and two flags: **120 bytes per draw** (measured: 7 680 B
  for 64), one heap block under tag `gfx3d`.
- Order: the opaque pass walks every visible draw in call order drawing only its non-blended submeshes; the blended
  pass walks the draws having a blended submesh, sorted back to front by the view depth of their sphere's centre
  (bottom-up stable merge sort), drawing only those submeshes. So a mixed model's glass waits for every opaque draw.
- Culling in view space (no plane extraction): near/far by the sphere's depth, sides by |x| − ahead·tan ≤ r·√(1+tan²)
  (tan from `sin_cos`), orthographic by half extents; a mesh destroyed mid-frame stays drawable that frame.
- `gfx/world_split.*` and `gfx_submit_slice` as planned; `gfx_build_pass` and `gfx_check_stale` are inline in
  `gfx_state.hpp`. **Merge watch-out**: `submit_screen` is now `hook → gfx_build_pass → gfx_submit_slice →
  gfx_check_stale`; 024's split dispatch goes inside `gfx_submit_slice`'s loop, 023's profile scope stays in
  `gfx_submit`.
- Golden (T4.10): kinds 6 (`to_screen`, perspective or orthographic, flags) and 7 (`screen_ray`) joined the script;
  frustum and order decisions are pinned by unit tests instead (the console can't reach them through the public API).
  **New golden `A3BF4EDF`** (checkpoints `95C33EC5`, `19D9D5B0`, `B657BA1D`, `C074FBC1`).
- No allocation over 100 frames of 256 draws (operator new and `nnh::mem` counted); no event published.
- **hello (P14) over budget**: 3DS Release `.text` 201 380 B = **+256 B** against the baseline (stage 2: +72 B for the
  hook). The extraction of `gfx_submit_slice` (an out-of-line function the 2D path now calls) costs ~180 B; the 3DS
  build doesn't garbage-collect sections, so any shared helper stays. Kept for the merge plan's single slice function;
  `.bss` +264 B (`TileSplit::layer` in the 64 split points); PC `.exe` +1 249 B (over the 1 KB budget by 225 B).
  Every example's screenshot unchanged.

### Stage 5 results and refinements (T5.1–T5.6)

- `engine/platforms/shared/raster3d/` as planned plus `raster3d.cpp` (clear, draw: vertex stage → clip → fan →
  coverage → depth → shade → cut-out/blend). The lighting tables live there (`light_tables.cpp`, pure) rather than in
  core's `gfx3d/`: platforms never call core, and the 3DS backend compiles the same file (stage 7).
- Clipping: near, far and a ±8 NDC guard band (so 28.4 coordinates stay far from overflow); a triangle crossing the
  camera plane becomes a fan.
- Fill rule: edge functions in int64 at pixel centres, top-left edges inclusive (on the y-down screen with the
  triangle normalised to a positive area: dy < 0, or dy = 0 and dx > 0); front faces are counter-clockwise in NDC.
- Shading: `surface.rgb × min(ambient + Σ diffuse, 1) + Σ specular`, clamped — the diffuse sum is clamped before the
  multiply, as the PICA200's fragment primary colour is. Depth: NDC z, less-than, cleared to +1.
- Fake pixels: `fake::enable_pixels_3d(true)`, `fake::pixel_3d(x, y)` (the picture of the frame that ended last);
  textures from `texture_create_rgba` copied, "FAKE" images become a gradient. 7 rasteriser tests + 2 end-to-end
  frames through core.

### Stage 6 results and refinements (T6.1–T6.5)

- `video3d_pc.cpp` holds init, meshes and the pass (no separate `pass3d_pc.cpp`/`mesh3d_pc.cpp`: ~150 lines);
  `texels3d_pc.cpp` reads a texture back once and keeps its pixels as an `SDL_Texture` property with a cleanup
  callback (freed with the texture, so no stale cache and no change to `texture_pc.cpp`).
- **Contract addition**: `const char* video3d_self_test()` — the backend's own check, logged by core after the first
  frame (Development; scripts can only expect lines from frame 1). PC: "3d tilt: PASS (simulator: no rotated screen to
  match)"; fake: none.
- T6.4: `scripts/check_png_hash.sh` (windowed default renderer, `NNH_SIM_RENDERER=sdl`, `SDL_VIDEO_DRIVER=offscreen`).
  The two headless runs give identical PNGs; the windowed GPU-device run differs — but it differs for 2D-only examples
  too (`examples/text`), so golden hashes are taken headless; the 3D pass itself is the same bytes on every renderer
  (P15: composed on the CPU).
- T6.5: the showcase's frames stay at 60 Hz in pc-debug on the maintainer's PC (688 triangles, 7 draws); P15's
  read-back fallback was not needed. A per-stage timing of the pass alone is not exposed (the profiler has no 3D scope
  yet: spec 023 adds a gfx submit scope).

### Stage 7 results and refinements (T7.1–T7.6)

- Files: `video3d_3ds.cpp` (target, shader, attribute layouts, meshes), `pass3d_3ds.cpp`, `lights3d_3ds.cpp`,
  `tilt3d_3ds.cpp`, `shaders/mesh3d.v.pica`; picasso + bin2s through `ctr_add_shader_library` and
  `dkp_add_embedded_binary_library` (header `nnh_mesh3d_shader_shbin.h`). 3ds-release and 3ds-dev build with no
  warning; `examples/models` produces a `.3dsx`.
- **Projection fix-up**: instead of a hand-written tilt/depth matrix, `F = Mtx_PerspTilt(60°, 400/240, 0.1, 100) ×
  inverse(GL(same))` is taken once from citro3d itself; the Development self-test compares `F × GL(p)` with
  `Mtx_PerspTilt(45°, …, 0.5, 50)` and `Mtx_OrthoTilt(±5, ±3, 1, 30)` (relative 1e-5) and logs `3d tilt: PASS|FAIL`.
- **Lighting tables in the PICA200's layouts** (shared file `engine/platforms/shared/raster3d/light_tables.cpp`, used by
  the simulator too): specular D0 on a signed N·H input, 128 entries over [0, 1) (x = i/128); distance attenuation
  unsigned, 256 entries (x = i/256), scale 1/range per light; citro3d's `LightLut_FromArray` gets values and
  differences. Highlights clamped where N·L ≤ 0 on both systems.
- Shader rule found while building: picasso refuses a uniform as the second source (`mul r, uniform, input` order).
- A material without vertex colours uses the fixed colour attribute (the loaders stop before the colour; the stride
  skips it); the colour scale is always base × tint / 255.
- Probes (T7.5) read back through `platform::remote_capture` after `end_frame` (Development); `test_probe.cpp` on fake.
- T7.6: `examples/models/tests/smoke.nnhs` (DOWN in the showcase opens the smoke scene: a flat cube, A → lit and
  textured) passes with `--pc`. Console run prepared for the lead (below, "Console runs").
- **Open points only the console can settle**: the cull winding after the tilt (`GPU_CULL_BACK_CCW`), the D0 signed
  layout, the alpha-test reference scale, depth GREATER after the frame-start clear to 0, the probe read-back of the
  top framebuffer.

### Stage 8 results (T8.1–T8.5)

- `examples/models`: `world.cpp` (two converted models — `ship.gltf` textured hull + red wings, `pylon.gltf` vertex
  colours — a cube, a sphere, a 12×12 checker floor, a cut-out fence, a blended glass pane), `showcase_scene.cpp` +
  `showcase_draw.cpp`, `check_scene.cpp` + `probe_scenes.cpp` (six probe scenes: flat, textured, lit, cut-out, blend over
  depth, 2D around 3D; expected colours worked out by hand from FR-10, no separate `shading_ref`), `bench_scene.cpp`,
  `smoke_scene.cpp`; art by `art/make_models.py`.
- PC: `3d: PASS A3BF4EDF`, `3d pixels: PASS` (all 15 probes); `tests/check.nnhs` passes with `--pc` (tag lines
  `200,88 depth 6.62`, orthographic `200,81 depth 6.62`); ctest runs `models_self_check_pc` and `models_check_script_pc`.
- The bench logs `rec` (the game's recording time) instead of the CPU time of the whole 3D work: culling, sorting and
  submission happen inside `nnh::frame()`; frame time avg/worst/missed carry the rest.

### Stage 9 results (T9.1, T9.2, T9.4; T9.3 — the Linux VM — is the lead's)

- T9.1: `nnh_tests` 2 674 tests (57 s, Windows pc-debug, other builds running), `nnh_assetc_tests` 275; ctest pc-debug
  273/273 (plus the two models tests added in stage 8), pc-release 259/259.
- T9.2 hand mutations (each applied, `nnh_tests` rebuilt, a filter run, reverted): **14 of 14 killed** — the eighth-turn
  snap off, the 2⁻¹⁰⁰ threshold moved, the blended sort's tie rule (`<` → `<=`), the top-left bias removed, the culling
  radius ignored, the quantisation's +½ removed, the index check `>=` → `>`, the affine check removed, the near-plane
  clip distance, the blend's +127 rounding, the normal matrix's determinant (the cofactors of a column), slerp's
  shortest-arc sign, the mirror wrap, the specular clamp where N·L ≤ 0.
- T9.4 (AC-9): `scripts/check_no_3d_symbols.sh` finds no 3D symbol in `hello` (3DS `.elf`, PC `.exe`) besides the hook
  pointer, and finds them in `models` (positive control). `hello`: 3DS Release `.text` 201 396 B (**+272 B**:
  +72 the hook, ~+184 the shared 2D slice function, +16 the command-buffer size in `app_3ds.cpp`), `.bss` +264 B,
  `.3dsx` 246 788 B (+276 B); PC Release `.exe` 279 942 B (+1 086 B). **Over P14's budget** (128 B / 1 KB) — reported to
  the maintainer; the remedy would be garbage-collected sections on 3DS or duplicating the 2D slice loop. Every other
  example's screenshot is unchanged (`scripts/example_shots.sh`, `audio` and `foundation` excluded: live timings);
  every earlier script passing in ctest. 2D-only games keep citro2d's targets (the 3D target replaces the top one only
  in `video3d_init`). Edit-one-game-`.cpp` times unchanged within noise (pc-debug 1.6 s, 3ds-release 3.0 s).
- T3.8's rebuild check (stage 9): touching `pylon.gltf` (pc-debug) or `ship.bin` (3ds-release) reconverts only that model
  and the asset index (`Model models/ship.gltf`, `Asset index for models`).

### Stage 10 (026a docs: T10.3, T10.4; the console run T10.1–T10.2 is the lead's)

- New pages: `docs/api/math3d.md`, `docs/api/mesh.md`, `docs/api/gfx3d.md`, guide `docs/guides/models.md`; links from
  `math.md`, `gfx.md`, `texture.md`, `assets.md`; `rendering.md` (the 3D pass, the world layer, the hook, the CPU
  rasteriser — the "3D will need a GPU device" text replaced), `sdl3.md` (D41: 3D on the CPU, the renderer comparison),
  `devkitpro-3ds.md` (target, shader, tables, combiners, projection fix-up, open points), `project-structure.md`
  (`platforms/shared/raster3d`, `platforms/3ds/shaders`), D41–D44 change-log line, `REFERENCES.md`, `CLAUDE.md`,
  ROADMAP M8 and `specs/README.md` (026a built, console pending).
