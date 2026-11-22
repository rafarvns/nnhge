#pragma once
// The `.nnhmap` format, version 1, which is also the in-memory layout of every nnh::Tilemap (spec 012,
// plan "Binary format"): one block, a 128-byte header, then sections of fixed-size entries, each 4-byte
// aligned, sized by the header's capacities. A map built in code and a loaded map are the same data.
//
// Pure: no engine includes, shared with the converter (tools/assetc). Little-endian (PC and 3DS both are).
// Offsets in the header are from the block start; layout rules live in map_layout.hpp.

#include <cstdint>

namespace nnh::detail::mapfmt {

inline constexpr char kMagic[4] = {'N', 'N', 'H', 'M'};
inline constexpr std::uint32_t kVersion = 1;

// Limits (checked by the builder, the loader and the converter).
inline constexpr int kMaxMapSize = 1024;        // cells per side
inline constexpr int kMinTileSize = 8;          // pixels per side
inline constexpr int kMaxTileSize = 64;
inline constexpr int kMaxTileLayers = 16;
inline constexpr int kMaxTilesets = 8;
inline constexpr int kMaxTileIds = 8191;        // across all tilesets; id 0 = empty
inline constexpr int kMaxObjectLayers = 16;
inline constexpr int kMaxObjects = 4096;
inline constexpr int kMaxProperties = 16384;
inline constexpr int kMaxStringBytes = 65536;   // the whole pool, including the empty string at offset 0
inline constexpr int kMaxStringLength = 255;    // bytes of one string, without its NUL
inline constexpr int kMaxAnimations = 256;
inline constexpr int kMaxFramesPerAnimation = 32;
inline constexpr int kMaxAnimationFrames = 4096;
inline constexpr int kMaxTotalCells = 4 * 1024 * 1024; // all tile layers together (8 MiB)

// Cells: u16, row-major per layer. Bits 0-12 tile id (0 = empty), 13 diagonal flip, 14 flip y, 15 flip x.
inline constexpr std::uint16_t kCellIdMask = 0x1FFF;
inline constexpr std::uint16_t kCellDiagonal = 1u << 13;
inline constexpr std::uint16_t kCellFlipY = 1u << 14;
inline constexpr std::uint16_t kCellFlipX = 1u << 15;

constexpr std::uint16_t encode_cell(int id, bool flip_x, bool flip_y, bool diagonal) {
    return static_cast<std::uint16_t>((static_cast<unsigned>(id) & kCellIdMask) | (diagonal ? kCellDiagonal : 0u) |
                                      (flip_y ? kCellFlipY : 0u) | (flip_x ? kCellFlipX : 0u));
}
constexpr int cell_id(std::uint16_t cell) { return cell & kCellIdMask; }
constexpr bool cell_flip_x(std::uint16_t cell) { return (cell & kCellFlipX) != 0; }
constexpr bool cell_flip_y(std::uint16_t cell) { return (cell & kCellFlipY) != 0; }
constexpr bool cell_diagonal(std::uint16_t cell) { return (cell & kCellDiagonal) != 0; }

// Tile flags (one byte per tile id). The collision bits hold collision::Cell's value (0 Empty, 1 Solid, 2 OneWay).
inline constexpr std::uint8_t kTileCollisionMask = 0x03;
inline constexpr std::uint8_t kTileAnimated = 0x04;      // stage 6
inline constexpr std::uint8_t kTileHasProperties = 0x08;

// Tile layer flags.
inline constexpr std::uint8_t kLayerVisible = 0x01;
inline constexpr std::uint8_t kLayerCollision = 0x02;

enum class PropertyType : std::uint8_t { None, Bool, Int, Float, String }; // same values as nnh::PropertyType
enum class ObjectShape : std::uint8_t { Rect, Point };                      // same values as nnh::ObjectShape

// Sections, in block order.
enum Section : int {
    kTilesets,     // TilesetEntry × tilesets capacity
    kTileSources,  // TileSource × (tile id capacity + 1), index = tile id
    kTileFlags,    // u8 × (tile id capacity + 1)
    kTileProps,    // TilePropsEntry × tile-props capacity, sorted by tile id
    kAnimations,   // AnimationEntry × animations capacity (stage 6)
    kFrames,       // FrameEntry × frames capacity (stage 6)
    kTileLayers,   // TileLayerEntry × tile layers capacity
    kCells,        // u16 × width × height × tile layers capacity
    kObjectLayers, // ObjectLayerEntry × object layers capacity
    kObjects,      // ObjectEntry × objects capacity, in creation order
    kProperties,   // PropertyEntry × properties capacity; each owner's properties are one contiguous range
    kStrings,      // NUL-terminated UTF-8, string capacity bytes; offset 0 is the empty string
    kSectionCount,
};

/// Strings are u16 offsets into the string section. Properties are ranges {first, count} into theirs.
struct Header {
    char magic[4];
    std::uint32_t version;
    std::uint32_t file_size; ///< bytes of the whole block / file
    std::uint32_t crc;       ///< CRC-32 of bytes [16, file_size) in files; 0 in memory
    std::uint16_t width, height;           ///< cells
    std::uint16_t tile_width, tile_height; ///< pixels
    float parallax_origin_x, parallax_origin_y;
    std::uint16_t properties_first, properties_count; ///< the map's own properties
    std::uint16_t collision_layers; ///< runtime: layers that may contribute to collision (bit per layer)
    std::uint16_t reserved0;
    std::uint8_t tilesets, tilesets_cap, tile_layers, tile_layers_cap;
    std::uint8_t object_layers, object_layers_cap;
    std::uint8_t owned_textures; ///< runtime: bit per tileset whose texture load_tilemap loaded (freed with the map)
    std::uint8_t reserved2;
    std::uint16_t tile_ids, tile_ids_cap; ///< ids 1..tile_ids are in use
    std::uint16_t tile_props, tile_props_cap;
    std::uint16_t animations, animations_cap;
    std::uint16_t frames, frames_cap;
    std::uint16_t objects, objects_cap;
    std::uint16_t properties, properties_cap;
    std::uint32_t string_bytes, string_cap; ///< used / capacity of the string section
    std::uint32_t offsets[kSectionCount];
};

struct TilesetEntry {
    std::uint16_t first_id, tile_count, columns, margin, spacing, image_width, image_height;
    std::uint16_t name;              ///< image asset name (files) or the name given in code
    std::int32_t texture_slot;       ///< runtime: the texture's handle (-1 / 0 in files)
    std::uint32_t texture_generation;
};

struct TileSource {
    std::uint16_t x, y; ///< top-left of the tile in its tileset's texture
};

struct TilePropsEntry {
    std::uint16_t tile_id, properties_first, properties_count;
};

struct AnimationEntry { // stage 6
    std::uint16_t tile_id;
    std::uint8_t frame_count, reserved;
    std::uint16_t first_frame, reserved2;
};

struct FrameEntry { // stage 6
    std::uint16_t tile_id, ms;
};

struct TileLayerEntry {
    std::uint16_t name;
    std::uint8_t flags;         ///< kLayerVisible, kLayerCollision
    std::uint8_t tilesets_used; ///< bit per tileset that may have tiles in this layer (never cleared)
    float offset_x, offset_y, parallax_x, parallax_y, opacity;
    std::uint16_t properties_first, properties_count;
    std::uint32_t cells; ///< offset of this layer's cells
    std::uint32_t reserved[2];
};

struct ObjectLayerEntry {
    std::uint16_t name, properties_first, properties_count, object_count;
    std::uint32_t reserved;
};

struct ObjectEntry {
    std::uint32_t id;
    std::uint16_t name, type;
    std::uint8_t shape, reserved;
    std::uint16_t object_layer;
    float x, y, w, h;
    std::uint16_t properties_first, properties_count;
    std::uint32_t reserved2;
};

struct PropertyEntry {
    std::uint16_t name;
    std::uint8_t type; ///< PropertyType
    std::uint8_t reserved;
    std::uint32_t value; ///< bool 0/1, int, float bits, or a string offset
    std::uint32_t reserved2;
};

static_assert(sizeof(Header) == 128);
static_assert(sizeof(TilesetEntry) == 24);
static_assert(sizeof(TileSource) == 4);
static_assert(sizeof(TilePropsEntry) == 6);
static_assert(sizeof(AnimationEntry) == 8);
static_assert(sizeof(FrameEntry) == 4);
static_assert(sizeof(TileLayerEntry) == 40);
static_assert(sizeof(ObjectLayerEntry) == 12);
static_assert(sizeof(ObjectEntry) == 36);
static_assert(sizeof(PropertyEntry) == 12);

} // namespace nnh::detail::mapfmt
