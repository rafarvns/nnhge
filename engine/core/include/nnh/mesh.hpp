#pragma once
/// @file nnh/mesh.hpp
/// Meshes and materials for 3D drawing (spec 026, decision D42): meshes built from the game's arrays or primitives,
/// models converted from glTF 2.0 at build time and loaded with load_mesh(), and the plain-value Material3D. Draw them
/// with <nnh/gfx3d.hpp>. See docs/api/mesh.md.
///
/// A mesh is 1..32 submeshes of triangles (16-bit indices, ≤ 65 535 vertices), each with a default material. Vertices
/// are quantised (12, 16 or 20 bytes: 16-bit positions over the mesh's bounds, 8-bit normals, 16-bit texture
/// coordinates, 8-bit colours) and live in the console's linear memory, accounted as `Linear`/`meshes` on every
/// system. Meshes need 3D on (AppConfig::gfx.max_draws_3d > 0): otherwise creating one fails with `Unsupported`.
/// Loading and creating allocate; nothing allocates afterwards. Mesh handles are RAII: destroying one frees its memory
/// at the end of the frame, and draws recorded with it are skipped.
///
/// @code
/// nnh::Result<nnh::Mesh> ship = nnh::load_mesh("models/ship");      // assets/models/ship.gltf, converted at build
/// nnh::Result<nnh::Mesh> ball = nnh::make_sphere(0.5f, 12, 24, {.base_color = nnh::colors::red});
/// @endcode

#include <cstddef>
#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/math3d.hpp"
#include "nnh/result.hpp"
#include "nnh/texture.hpp"

namespace nnh {

enum class AlphaMode : std::uint8_t {
    Opaque, ///< alpha ignored
    Cutout, ///< pixels with alpha below Material3D::alpha_cutoff are discarded (leaves, fences)
    Blend,  ///< mixed with what is behind; drawn after opaque draws, back to front, no depth write
};
enum class TextureFilter : std::uint8_t { Nearest, Linear };
enum class TextureWrap : std::uint8_t { Repeat, Clamp, Mirror };

/// A texture named by value (non-owning): its slot and generation, from texture_id(). {} = no texture. A material whose
/// texture was destroyed draws untextured (and asserts in Debug/Development).
struct TextureId {
    int slot = -1;
    std::uint32_t generation = 0;
    friend constexpr bool operator==(TextureId a, TextureId b) { return a.slot == b.slot && a.generation == b.generation; }
};
/// The TextureId of a loaded texture (an invalid texture gives {}).
TextureId texture_id(const Texture& texture);

inline constexpr int kMaxShininess = 7;     ///< shininess levels 0..7 = exponents 2, 4, …, 256
inline constexpr int kMaxSubmeshes = 32;    ///< per mesh
inline constexpr int kMaxMeshVertices = 65535;
inline constexpr int kMaxMeshTextures = 8;  ///< textures a converted model may use

/// How a surface looks (FR-10): `surface = texture × vertex colour × base colour`; unlit: surface; lit: surface.rgb ×
/// (ambient + Σ diffuse) + Σ specular × specular colour × (N·H)^(2^(shininess+1)). Plain, trivially copyable.
struct Material3D {
    Color base_color = colors::white;
    Color specular = colors::black;   ///< rgb used; black = no highlight
    TextureId texture{};              ///< {} = untextured
    std::uint8_t shininess = 3;       ///< level 0..7: exponent 2^(level+1) (16 by default)
    AlphaMode alpha = AlphaMode::Opaque;
    std::uint8_t alpha_cutoff = 128;  ///< Cutout: alpha < cutoff → discarded
    TextureFilter filter = TextureFilter::Nearest;
    TextureWrap wrap_u = TextureWrap::Repeat;
    TextureWrap wrap_v = TextureWrap::Repeat;
    bool lit = true;                  ///< false: the surface colour as is
    bool vertex_colors = true;        ///< multiply the mesh's vertex colours (white when it has none)
    bool cull_back = true;            ///< skip triangles facing away (counter-clockwise = front)
};

struct Aabb3 {
    Vec3 min{};
    Vec3 max{};
};
struct Sphere {
    Vec3 center{};
    float radius = 0;
};

/// One submesh of a MeshDesc: `index_count` indices (a multiple of 3) from `first_index` (a multiple of 3).
struct SubmeshDesc {
    int first_index = 0;
    int index_count = 0;
    Material3D material{};
};

/// The game's arrays for create_mesh(), read once. Front faces are counter-clockwise.
struct MeshDesc {
    const Vec3* positions = nullptr; ///< required
    int vertex_count = 0;            ///< 1..65535
    const Vec3* normals = nullptr;   ///< null → flat normals (vertices unwelded per triangle)
    const Vec2* uvs = nullptr;       ///< null → no texture coordinates (textures sample (0, 0))
    const Color* colors = nullptr;   ///< null → white, no colour attribute
    const std::uint16_t* indices = nullptr; ///< null → 0, 1, 2, … (vertex_count must then be a multiple of 3)
    int index_count = 0;
    const SubmeshDesc* submeshes = nullptr; ///< null → one submesh of every index, default material
    int submesh_count = 0;
};

/// A loaded or created mesh (move-only RAII handle). Draw it with gfx3d::draw().
class Mesh {
public:
    Mesh() = default;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    ~Mesh();

    bool valid() const;          ///< false when empty, reset or created before the engine shut down
    int submesh_count() const;   ///< 0 when invalid
    int vertex_count() const;
    int triangle_count() const;
    Aabb3 bounds() const;        ///< local bounds
    Sphere bounding_sphere() const;
    Material3D material(int submesh) const; ///< the default material of a submesh ({} when out of range)
    void reset();                ///< frees the mesh now (draws recorded with it are skipped)

private:
    friend struct MeshAccess;
    int slot_ = -1;
    std::uint32_t generation_ = 0;
};

/// A mesh from the game's arrays, quantised exactly as nnh-assetc quantises the same data (the same bytes). Refuses
/// (`InvalidArgument`, a log line saying why) out-of-range counts or indices, non-finite values, invalid materials;
/// `Unsupported` with 3D off; `OutOfMemory`. Load time only (allocates).
Result<Mesh> create_mesh(const MeshDesc& desc);
/// A cube of side `size` centred on the origin: 24 vertices, 12 triangles, normals, texture coordinates 0..1 per face.
Result<Mesh> make_cube(float size = 1, const Material3D& material = {});
/// A plane in x/z centred on the origin facing +y, `subdivisions` (1..64) cells per side, texture coordinates 0..1.
Result<Mesh> make_plane(float width, float depth, int subdivisions = 1, const Material3D& material = {});
/// A UV sphere: `rings` (2..64) bands from pole to pole, `segments` (3..128) around, normals, texture coordinates.
Result<Mesh> make_sphere(float radius, int rings = 12, int segments = 24, const Material3D& material = {});
/// A converted model from the assets: "models/ship" reads models/ship.nnhmesh (made from ship.gltf or ship.glb by the
/// build) and loads its textures (owned by the mesh). `NotFound`, `Corrupt` (validated, never trusted), `Unsupported`
/// with 3D off, `OutOfMemory`. Load time only.
Result<Mesh> load_mesh(const char* path);

} // namespace nnh
