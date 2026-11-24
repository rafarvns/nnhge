#pragma once
// The .nnhmesh file, version 1 (spec 026, decision D42). Little-endian (PC and 3DS), every field naturally aligned, so
// the structs below are the bytes (static_asserts). Pure: shared by core (create_mesh, load_mesh) and nnh-assetc.
//
//   header      112 B   MeshHeader
//   submeshes   48 B each, from offsets[0]
//   textures    64 B each, from offsets[1]: asset path without ".png", NUL-padded
//   vertices    stride B each, from offsets[2]: position int16×4 (x, y, z, 0) · normal int8×4 (x, y, z, 0) ·
//               [uv int16×2 when kFlagUv] · [colour u8×4 when kFlagColor]
//   indices     u16 each, from offsets[3] (+2 B of padding when odd)
//
// A position decodes as q × pos_scale + pos_offset (per axis); a normal as q / 127; a texture coordinate as
// q × uv_scale + uv_offset of its submesh; a colour as q / 255.

#include <cstddef>
#include <cstdint>

namespace nnh::detail::mesh {

inline constexpr char kMagic[4] = {'N', 'N', 'H', 'M'};
inline constexpr std::uint32_t kVersion = 1;
inline constexpr std::uint8_t kFlagUv = 1;
inline constexpr std::uint8_t kFlagColor = 2;
inline constexpr int kMaxIndices = 786432; ///< 262 144 triangles
inline constexpr int kMaxSubmeshes = 32;
inline constexpr int kMaxTextures = 8;
inline constexpr int kPathBytes = 64;
inline constexpr std::uint8_t kNoTexture = 0xFF;

// MaterialRecord::flags
inline constexpr std::uint8_t kMatLit = 1;
inline constexpr std::uint8_t kMatVertexColors = 2;
inline constexpr std::uint8_t kMatCullBack = 4;

struct MeshHeader {
    char magic[4];
    std::uint32_t version;
    std::uint32_t header_size; ///< sizeof(MeshHeader)
    std::uint32_t file_size;
    std::uint32_t crc;         ///< CRC-32 of the bytes after the header
    std::uint8_t flags;        ///< kFlagUv | kFlagColor
    std::uint8_t submesh_count;
    std::uint8_t texture_count;
    std::uint8_t stride;       ///< 12, 16 or 20
    std::uint32_t vertex_count;
    std::uint32_t index_count;
    float pos_scale[3];
    float pos_offset[3];
    float aabb_min[3];
    float aabb_max[3];
    float sphere[4];           ///< centre, radius
    std::uint32_t offsets[4];  ///< submeshes, textures, vertices, indices
};

struct MaterialRecord {
    std::uint8_t base[4];      ///< RGBA
    std::uint8_t specular[3];  ///< RGB
    std::uint8_t shininess;    ///< 0..7
    std::uint8_t texture;      ///< index into the textures, or kNoTexture
    std::uint8_t alpha;        ///< AlphaMode
    std::uint8_t cutoff;
    std::uint8_t filter;       ///< TextureFilter
    std::uint8_t wrap_u;       ///< TextureWrap
    std::uint8_t wrap_v;
    std::uint8_t flags;        ///< kMatLit | kMatVertexColors | kMatCullBack
    std::uint8_t reserved;     ///< 0
};

struct SubmeshRecord {
    std::uint32_t first_index;
    std::uint32_t index_count;
    std::uint16_t min_vertex;
    std::uint16_t max_vertex;
    float uv_scale[2];
    float uv_offset[2];
    MaterialRecord material;
    std::uint32_t reserved;    ///< 0
};

static_assert(sizeof(MeshHeader) == 112, "the .nnhmesh header is 112 bytes");
static_assert(sizeof(MaterialRecord) == 16, "a material record is 16 bytes");
static_assert(sizeof(SubmeshRecord) == 48, "a submesh record is 48 bytes");

/// The vertex size for a mesh's flags: 12 + 4 per optional attribute.
constexpr int vertex_stride(std::uint8_t flags) {
    return 12 + ((flags & kFlagUv) != 0 ? 4 : 0) + ((flags & kFlagColor) != 0 ? 4 : 0);
}
/// Byte offsets of the optional attributes inside a vertex (-1 when absent).
constexpr int uv_offset_in_vertex(std::uint8_t flags) { return (flags & kFlagUv) != 0 ? 12 : -1; }
constexpr int color_offset_in_vertex(std::uint8_t flags) {
    return (flags & kFlagColor) == 0 ? -1 : ((flags & kFlagUv) != 0 ? 16 : 12);
}

/// Where each section starts and the file's size, for the given counts.
struct Layout {
    std::uint32_t offsets[4] = {};
    std::uint32_t file_size = 0;
};
Layout layout(int submesh_count, int texture_count, int vertex_count, int stride, int index_count);

} // namespace nnh::detail::mesh
