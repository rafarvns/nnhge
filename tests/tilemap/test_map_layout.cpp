// Map format and layout (spec 012 FR-11): cell bits, limits, capacities → sections and memory formula.

#include <cstdint>

#include "test.hpp"
#include "tilemap/map_format.hpp"
#include "tilemap/map_layout.hpp"

namespace fmt = nnh::detail::mapfmt;
using nnh::detail::map_layout;
using nnh::detail::MapCapacities;
using nnh::detail::MapLayout;

namespace {

std::uint32_t align4(std::uint32_t n) { return (n + 3u) & ~3u; }

// The plan's memory formula, written out independently of the layout code.
std::uint32_t formula(const MapCapacities& c) {
    const auto u = [](int v) { return static_cast<std::uint32_t>(v); };
    const std::uint32_t ids = u(c.tile_ids) + 1;
    return 128 + 24 * u(c.tilesets) + 4 * ids + align4(ids) + align4(6 * u(c.tile_props)) + 8 * u(c.animations) +
           4 * u(c.animation_frames) + 40 * u(c.tile_layers) + align4(2 * u(c.width) * u(c.height) * u(c.tile_layers)) +
           12 * u(c.object_layers) + 36 * u(c.objects) + 12 * u(c.properties) + align4(u(c.string_bytes));
}

MapCapacities odd_sizes() {
    return {.width = 3, .height = 5, .tilesets = 1, .tile_ids = 6, .tile_props = 1, .animations = 0,
            .animation_frames = 0, .tile_layers = 1, .object_layers = 1, .objects = 1, .properties = 3,
            .string_bytes = 5};
}

} // namespace

NNH_TEST(map_format_entries_have_the_planned_sizes) {
    CHECK(sizeof(fmt::Header) == 128);
    CHECK(sizeof(fmt::TilesetEntry) == 24);
    CHECK(sizeof(fmt::TileSource) == 4);
    CHECK(sizeof(fmt::TilePropsEntry) == 6);
    CHECK(sizeof(fmt::AnimationEntry) == 8);
    CHECK(sizeof(fmt::FrameEntry) == 4);
    CHECK(sizeof(fmt::TileLayerEntry) == 40);
    CHECK(sizeof(fmt::ObjectLayerEntry) == 12);
    CHECK(sizeof(fmt::ObjectEntry) == 36);
    CHECK(sizeof(fmt::PropertyEntry) == 12);
}

NNH_TEST(map_cell_bits_put_the_id_low_and_the_flips_high) {
    CHECK(fmt::encode_cell(1, false, false, false) == 0x0001);
    CHECK(fmt::encode_cell(8191, false, false, false) == 0x1FFF);
    CHECK(fmt::encode_cell(0, false, false, true) == 0x2000);
    CHECK(fmt::encode_cell(0, false, true, false) == 0x4000);
    CHECK(fmt::encode_cell(0, true, false, false) == 0x8000);
    CHECK(fmt::cell_id(0xFFFF) == 8191);
    CHECK(fmt::cell_flip_x(0x8000) && !fmt::cell_flip_y(0x8000) && !fmt::cell_diagonal(0x8000));
    CHECK(fmt::cell_flip_y(0x4000) && !fmt::cell_flip_x(0x4000));
    CHECK(fmt::cell_diagonal(0x2000) && !fmt::cell_flip_y(0x2000));
}

NNH_TEST(map_cell_ids_above_the_limit_are_masked) {
    CHECK(fmt::encode_cell(8192, false, false, false) == 0); // 0x2000 & 0x1FFF: never a stray flip bit
}

NNH_TEST(map_layout_of_no_capacity_is_just_the_header) {
    const MapLayout l = map_layout({});
    CHECK(l.total == 128 + 4 + 4); // the tile table always has entry 0 (empty): 4 B source + flags padded
    CHECK(l.total == formula({}));
}

NNH_TEST(map_layout_size_matches_the_memory_formula) {
    CHECK(map_layout(odd_sizes()).total == formula(odd_sizes()));
    const MapCapacities level{.width = 240, .height = 30, .tilesets = 1, .tile_ids = 128, .tile_props = 8,
                              .animations = 0, .animation_frames = 0, .tile_layers = 3, .object_layers = 1,
                              .objects = 40, .properties = 60, .string_bytes = 1024};
    CHECK(map_layout(level).total == formula(level));
    const MapCapacities big{.width = 1024, .height = 1024, .tilesets = 8, .tile_ids = 8191, .tile_props = 8191,
                            .animations = 256, .animation_frames = 4096, .tile_layers = 4, .object_layers = 16,
                            .objects = 4096, .properties = 16384, .string_bytes = 65536};
    CHECK(map_layout(big).total == formula(big));
}

NNH_TEST(map_layout_sections_are_aligned_ordered_and_contiguous) {
    const MapLayout l = map_layout(odd_sizes());
    CHECK(l.offsets[0] == 128);
    for (int s = 0; s < fmt::kSectionCount; ++s) CHECK(l.offsets[s] % 4 == 0);
    for (int s = 0; s + 1 < fmt::kSectionCount; ++s) CHECK(l.offsets[s] + l.sizes[s] == l.offsets[s + 1]);
    const int last = fmt::kSectionCount - 1;
    CHECK(l.offsets[last] + l.sizes[last] == l.total);
}

NNH_TEST(map_layout_section_sizes_follow_their_entries) {
    const MapLayout l = map_layout(odd_sizes());
    CHECK(l.sizes[fmt::kTilesets] == 24);
    CHECK(l.sizes[fmt::kTileSources] == 4 * 7);
    CHECK(l.sizes[fmt::kTileFlags] == 8);  // 7 flag bytes, padded
    CHECK(l.sizes[fmt::kTileProps] == 8);  // 6 bytes, padded
    CHECK(l.sizes[fmt::kTileLayers] == 40);
    CHECK(l.sizes[fmt::kCells] == 32);     // 3 x 5 x 2 = 30, padded
    CHECK(l.sizes[fmt::kObjectLayers] == 12);
    CHECK(l.sizes[fmt::kObjects] == 36);
    CHECK(l.sizes[fmt::kProperties] == 36);
    CHECK(l.sizes[fmt::kStrings] == 8);
    CHECK(l.sizes[fmt::kAnimations] == 0 && l.sizes[fmt::kFrames] == 0);
}

NNH_TEST(map_capacities_within_the_limits_are_accepted) {
    CHECK(nnh::detail::check_capacities(odd_sizes()).ok);
    const MapCapacities max{.width = 1024, .height = 1024, .tilesets = 8, .tile_ids = 8191, .tile_props = 8191,
                            .animations = 256, .animation_frames = 4096, .tile_layers = 4, .object_layers = 16,
                            .objects = 4096, .properties = 16384, .string_bytes = 65536};
    CHECK(nnh::detail::check_capacities(max).ok);
}

NNH_TEST(map_capacities_reject_bad_sizes_as_invalid) {
    MapCapacities c = odd_sizes();
    c.width = 0;
    auto r = nnh::detail::check_capacities(c);
    CHECK(!r.ok && !r.limit);
    c = odd_sizes();
    c.height = -1;
    CHECK(!nnh::detail::check_capacities(c).ok && !nnh::detail::check_capacities(c).limit);
    c = odd_sizes();
    c.objects = -1;
    CHECK(!nnh::detail::check_capacities(c).ok && !nnh::detail::check_capacities(c).limit);
}

NNH_TEST(map_capacities_over_the_engine_limits_are_refused_as_limits) {
    const auto over = [](MapCapacities c) {
        const auto r = nnh::detail::check_capacities(c);
        return !r.ok && r.limit && r.reason != nullptr;
    };
    MapCapacities c = odd_sizes();
    c.width = 1025;
    CHECK(over(c));
    c = odd_sizes();
    c.tile_layers = 17;
    CHECK(over(c));
    c = odd_sizes();
    c.tilesets = 9;
    CHECK(over(c));
    c = odd_sizes();
    c.tile_ids = 8192;
    CHECK(over(c));
    c = odd_sizes();
    c.object_layers = 17;
    CHECK(over(c));
    c = odd_sizes();
    c.objects = 4097;
    CHECK(over(c));
    c = odd_sizes();
    c.properties = 16385;
    CHECK(over(c));
    c = odd_sizes();
    c.string_bytes = 65537;
    CHECK(over(c));
    c = odd_sizes();
    c.animations = 257;
    CHECK(over(c));
    c = odd_sizes();
    c.animation_frames = 4097;
    CHECK(over(c));
}

NNH_TEST(map_capacities_cap_the_total_cells_of_all_layers) {
    MapCapacities c = odd_sizes();
    c.width = 1024;
    c.height = 1024;
    c.tile_layers = 4; // exactly 4 Mi cells
    CHECK(nnh::detail::check_capacities(c).ok);
    c.tile_layers = 5;
    CHECK(!nnh::detail::check_capacities(c).ok && nnh::detail::check_capacities(c).limit);
    c.width = 512;
    c.height = 512;
    c.tile_layers = 16; // also 4 Mi
    CHECK(nnh::detail::check_capacities(c).ok);
}

NNH_TEST(a_fresh_map_block_has_its_header_offsets_and_layer_cells) {
    const MapCapacities c{.width = 3, .height = 5, .tile_ids = 6, .tile_layers = 2, .string_bytes = 5};
    const MapLayout l = map_layout(c);
    alignas(8) static std::uint8_t block[512] = {};
    CHECK(l.total <= sizeof block);
    nnh::detail::init_map_block(block, c, 16, 8);
    const nnh::detail::MapView view{block};
    const fmt::Header& h = view.header();
    CHECK(h.magic[0] == 'N' && h.magic[3] == 'M' && h.version == 1 && h.file_size == l.total);
    CHECK(h.width == 3 && h.height == 5 && h.tile_width == 16 && h.tile_height == 8);
    CHECK(h.tile_layers == 0 && h.tile_layers_cap == 2 && h.tile_ids_cap == 6 && h.string_cap == 5);
    CHECK(h.string_bytes == 1 && view.string(0)[0] == '\0');
    for (int s = 0; s < fmt::kSectionCount; ++s) CHECK(h.offsets[s] == l.offsets[s]);
    CHECK(view.tile_layers()[0].cells == l.offsets[fmt::kCells]);
    CHECK(view.tile_layers()[1].cells == l.offsets[fmt::kCells] + 30);
}
