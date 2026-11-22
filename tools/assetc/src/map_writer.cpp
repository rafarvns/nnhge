// The map model → a `.nnhmap` file (see map_writer.hpp). Uses core's pure layout, string pool and property table, so
// the bytes are laid out exactly as a map built in code.

#include "map_writer.hpp"

#include <algorithm>
#include <climits>
#include <cstring>
#include <set>

#include "core/crc32.hpp"
#include "tilemap/map_layout.hpp"
#include "tilemap/map_property_table.hpp"
#include "tilemap/map_strings.hpp"
#include "tilemap/map_validate.hpp"

namespace nnh::assetc {

namespace {

using namespace nnh::detail;
namespace fmt = nnh::detail::mapfmt;

std::uint16_t u16(int v) { return static_cast<std::uint16_t>(v); }

/// The string pool's size (with its empty string), or false when a string is over 255 bytes.
bool pool_size(const Map& m, std::size_t& size, std::string& error) {
    std::set<std::string> unique;
    bool ok = true;
    auto add = [&](const std::string& s) {
        if (s.size() > static_cast<std::size_t>(fmt::kMaxStringLength)) {
            if (ok) error = "\"" + s.substr(0, 32) + "...\" is longer than 255 bytes";
            ok = false;
        } else if (!s.empty()) {
            unique.insert(s);
        }
    };
    auto add_props = [&](const Properties& props) {
        for (const Property& p : props) {
            add(p.name);
            if (p.type == fmt::PropertyType::String) add(p.text);
        }
    };
    add_props(m.properties);
    for (const Tileset& s : m.tilesets) {
        add(s.image);
        for (const auto& tile : s.tiles) add_props(tile.second.properties);
    }
    for (const TileLayer& l : m.tile_layers) {
        add(l.name);
        add_props(l.properties);
    }
    for (const ObjectLayer& l : m.object_layers) {
        add(l.name);
        add_props(l.properties);
    }
    for (const Object& o : m.objects) {
        add(o.name);
        add(o.type);
        add_props(o.properties);
    }
    size = 1;
    for (const std::string& s : unique) size += s.size() + 1;
    return ok;
}

MapCapacities capacities(const Map& m, std::size_t pool) {
    MapCapacities c;
    c.width = m.width;
    c.height = m.height;
    c.tilesets = static_cast<int>(m.tilesets.size());
    std::size_t properties = m.properties.size();
    for (const Tileset& s : m.tilesets) {
        c.tile_ids += s.tile_count;
        for (const auto& tile : s.tiles) {
            c.tile_props += tile.second.properties.empty() ? 0 : 1;
            properties += tile.second.properties.size();
        }
    }
    for (const TileLayer& l : m.tile_layers) properties += l.properties.size();
    for (const ObjectLayer& l : m.object_layers) properties += l.properties.size();
    for (const Object& o : m.objects) properties += o.properties.size();
    c.tile_layers = static_cast<int>(m.tile_layers.size());
    c.object_layers = static_cast<int>(m.object_layers.size());
    c.objects = static_cast<int>(std::min<std::size_t>(m.objects.size(), INT_MAX));
    c.properties = static_cast<int>(std::min<std::size_t>(properties, INT_MAX));
    c.string_bytes = static_cast<int>(std::min<std::size_t>(pool, INT_MAX));
    return c;
}

struct Writer {
    MapView v;

    std::uint16_t str(const std::string& s) const { return u16(map_intern(v, s.c_str())); } // room is counted

    void props(PropertyRange range, const Properties& list) const {
        for (const Property& p : list) {
            fmt::PropertyEntry& e = v.properties()[insert_property(v, range)];
            e.name = str(p.name);
            e.type = static_cast<std::uint8_t>(p.type);
            switch (p.type) {
            case fmt::PropertyType::Bool: e.value = p.boolean ? 1u : 0u; break;
            case fmt::PropertyType::Int: e.value = static_cast<std::uint32_t>(p.integer); break;
            case fmt::PropertyType::Float: std::memcpy(&e.value, &p.real, sizeof e.value); break;
            default: e.value = str(p.text); break;
            }
        }
    }

    void tilesets(const Map& m) const {
        fmt::Header& h = v.header();
        for (const Tileset& s : m.tilesets) {
            fmt::TilesetEntry& e = v.tilesets()[h.tilesets++];
            e = {u16(s.first_id), u16(s.tile_count), u16(s.columns), u16(s.margin), u16(s.spacing),
                 u16(s.image_width), u16(s.image_height), str(s.image), -1, 0};
            for (int t = 0; t < s.tile_count; ++t) {
                v.tile_sources()[s.first_id + t] = tile_source(e, t, m.tile_width, m.tile_height);
            }
            for (const auto& [local, info] : s.tiles) { // ascending ids: entries are appended in order
                const int id = s.first_id + local;
                v.tile_flags()[id] = info.collision;
                if (info.properties.empty()) continue;
                fmt::TilePropsEntry& t = v.tile_props()[add_tile_props(v, id)];
                props({&t.properties_first, &t.properties_count}, info.properties);
            }
            h.tile_ids = u16(h.tile_ids + s.tile_count);
        }
    }

    void tile_layers(const Map& m) const {
        fmt::Header& h = v.header();
        for (const TileLayer& l : m.tile_layers) {
            const int index = h.tile_layers++;
            fmt::TileLayerEntry& e = v.tile_layers()[index];
            e.name = str(l.name);
            e.flags = static_cast<std::uint8_t>((l.visible ? fmt::kLayerVisible : 0) | (l.collision ? fmt::kLayerCollision : 0));
            e.offset_x = l.offset_x;
            e.offset_y = l.offset_y;
            e.parallax_x = l.parallax_x;
            e.parallax_y = l.parallax_y;
            e.opacity = l.opacity;
            std::memcpy(v.cells(index), l.cells.data(), l.cells.size() * sizeof(std::uint16_t));
            for (const std::uint16_t cell : l.cells) {
                const int id = fmt::cell_id(cell);
                for (int s = 0; id != 0 && s < h.tilesets; ++s) {
                    const fmt::TilesetEntry& set = v.tilesets()[s];
                    if (id >= set.first_id && id < set.first_id + set.tile_count) {
                        e.tilesets_used = static_cast<std::uint8_t>(e.tilesets_used | (1u << s));
                    }
                }
            }
            props({&e.properties_first, &e.properties_count}, l.properties);
        }
    }

    void objects(const Map& m) const {
        fmt::Header& h = v.header();
        for (const ObjectLayer& l : m.object_layers) {
            fmt::ObjectLayerEntry& e = v.object_layers()[h.object_layers++];
            e.name = str(l.name);
            props({&e.properties_first, &e.properties_count}, l.properties);
        }
        for (const Object& o : m.objects) {
            fmt::ObjectEntry& e = v.objects()[h.objects++];
            e.id = o.id;
            e.name = str(o.name);
            e.type = str(o.type);
            e.shape = static_cast<std::uint8_t>(o.shape);
            e.object_layer = u16(o.object_layer);
            e.x = o.x;
            e.y = o.y;
            e.w = o.w;
            e.h = o.h;
            ++v.object_layers()[o.object_layer].object_count;
            props({&e.properties_first, &e.properties_count}, o.properties);
        }
    }
};

} // namespace

bool write_map(const Map& map, std::vector<std::uint8_t>& out, std::string& error) {
    out.clear();
    std::size_t pool = 0;
    if (!pool_size(map, pool, error)) return false;
    const MapCapacities caps = capacities(map, pool);
    const CapacityCheck check = check_capacities(caps);
    if (!check.ok) {
        error = std::string("the map has ") + check.reason + " (a limit of the format)";
        return false;
    }
    out.assign(map_layout(caps).total, 0);
    init_map_block(out.data(), caps, map.tile_width, map.tile_height);
    const Writer w{MapView{out.data()}};
    fmt::Header& h = w.v.header();
    h.parallax_origin_x = map.parallax_origin_x;
    h.parallax_origin_y = map.parallax_origin_y;
    w.props({&h.properties_first, &h.properties_count}, map.properties);
    w.tilesets(map);
    w.tile_layers(map);
    w.objects(map);
    h.crc = crc32(out.data() + 16, out.size() - 16);
    const MapCheck valid = validate_map(out.data(), out.size());
    if (!valid.ok()) {
        error = std::string("the written map fails the engine's check (") + valid.reason +
                "): this is a converter bug, please report it";
        out.clear();
        return false;
    }
    return true;
}

} // namespace nnh::assetc
