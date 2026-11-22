#pragma once
// A Tiled map as nnh-assetc understands it, between reading the JSON (tiled_*.cpp) and writing the `.nnhmap`
// (map_writer.cpp). Values are already checked against the format's rules; ids are NNHGE's (1.. across tilesets).

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "tilemap/map_format.hpp"

namespace nnh::assetc {

struct Property {
    std::string name;
    nnh::detail::mapfmt::PropertyType type = nnh::detail::mapfmt::PropertyType::String;
    bool boolean = false;
    std::int32_t integer = 0;
    float real = 0;
    std::string text;
};
using Properties = std::vector<Property>;

struct AnimationFrame {
    int tile = 0; ///< the tile shown: a local id of the same tileset
    int ms = 0;   ///< 1..65535
};

struct TileInfo {
    std::uint8_t collision = 0; ///< collision::Cell value: 0 Empty, 1 Solid (`solid`), 2 OneWay (`one_way`)
    Properties properties;      ///< the tile's other properties
    std::vector<AnimationFrame> animation; ///< Tiled's frames (1..32), or empty: not animated
};

struct Tileset {
    int first_gid = 0; ///< Tiled's first global id
    int first_id = 0;  ///< NNHGE's first tile id (set when the map is assembled)
    int tile_count = 0, columns = 0, margin = 0, spacing = 0;
    int image_width = 0, image_height = 0;
    std::string image; ///< asset name of the image, without ".png" (what load_texture takes)
    std::map<int, TileInfo> tiles; ///< by local tile id, only tiles with collision, properties or an animation
};

struct TileLayer {
    std::string name;
    float offset_x = 0, offset_y = 0, parallax_x = 1, parallax_y = 1, opacity = 1;
    bool visible = true;
    bool collision = false; ///< from the layer property `collision`
    Properties properties;  ///< the other properties
    std::vector<std::uint16_t> cells; ///< width × height, row-major, NNHGE cell encoding
};

struct ObjectLayer {
    std::string name;
    Properties properties;
};

struct Object {
    std::uint32_t id = 0;
    std::string name, type;
    nnh::detail::mapfmt::ObjectShape shape = nnh::detail::mapfmt::ObjectShape::Rect;
    float x = 0, y = 0, w = 0, h = 0;
    int object_layer = 0;
    Properties properties;
};

struct Map {
    int width = 0, height = 0, tile_width = 0, tile_height = 0;
    float parallax_origin_x = 0, parallax_origin_y = 0;
    Properties properties;
    std::vector<Tileset> tilesets; ///< in Tiled's first-gid order
    std::vector<TileLayer> tile_layers;
    std::vector<ObjectLayer> object_layers;
    std::vector<Object> objects;
};

} // namespace nnh::assetc
