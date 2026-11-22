// Validation of `.nnhmap` files, part 2 (spec 012 FR-1, constitution X): tile layers and cells, object layers
// and objects, properties and property ranges. Header, strings and tilesets: test_map_validate.cpp.

#include <cmath>
#include <cstring>
#include <limits>

#include "map_file.hpp"
#include "test.hpp"
#include "tilemap/map_validate.hpp"

using map_file::Bytes;
using nnh::detail::MapProblem;
using nnh::detail::validate_map;
namespace fmt = nnh::detail::mapfmt;

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

bool rejects(Bytes& file, const char* words) {
    map_file::seal(file);
    const nnh::detail::MapCheck c = validate_map(file.data(), file.size());
    return c.problem == MapProblem::Damaged && std::strstr(c.reason, words) != nullptr;
}

bool accepts(Bytes& file) {
    map_file::seal(file);
    return validate_map(file.data(), file.size()).ok();
}

nnh::detail::MapView view(Bytes& file) { return map_file::view(file); }

} // namespace

// --- tile layers and cells ---

NNH_TEST(map_validate_refuses_unknown_tile_layer_flags) {
    Bytes file = map_file::sample();
    view(file).tile_layers()[0].flags = 0x04;
    CHECK(rejects(file, "layer flags"));
}

NNH_TEST(map_validate_refuses_layer_opacity_outside_0_to_1) {
    const float bad[] = {-0.01f, 1.01f, kNaN};
    for (float opacity : bad) {
        Bytes file = map_file::sample();
        view(file).tile_layers()[1].opacity = opacity;
        CHECK(rejects(file, "opacity"));
    }
    Bytes file = map_file::sample();
    view(file).tile_layers()[1].opacity = 0;
    CHECK(accepts(file));
}

NNH_TEST(map_validate_refuses_layer_offsets_and_parallax_that_are_not_finite) {
    for (int field = 0; field < 4; ++field) {
        Bytes file = map_file::sample();
        fmt::TileLayerEntry& e = view(file).tile_layers()[0];
        float* values[] = {&e.offset_x, &e.offset_y, &e.parallax_x, &e.parallax_y};
        *values[field] = field % 2 == 0 ? kInf : kNaN;
        CHECK(rejects(file, "finite"));
    }
}

NNH_TEST(map_validate_refuses_a_parallax_origin_that_is_not_finite) {
    Bytes file = map_file::sample();
    view(file).header().parallax_origin_y = -kInf;
    CHECK(rejects(file, "parallax origin"));
    file = map_file::sample();
    view(file).header().parallax_origin_x = kNaN;
    CHECK(rejects(file, "parallax origin"));
}

NNH_TEST(map_validate_refuses_names_outside_the_string_pool) {
    struct Case {
        void (*damage)(const nnh::detail::MapView&, std::uint16_t past_end);
    };
    const Case cases[] = {
        {[](const nnh::detail::MapView& v, std::uint16_t at) { v.tile_layers()[1].name = at; }},
        {[](const nnh::detail::MapView& v, std::uint16_t at) { v.object_layers()[0].name = at; }},
        {[](const nnh::detail::MapView& v, std::uint16_t at) { v.objects()[1].name = at; }},
        {[](const nnh::detail::MapView& v, std::uint16_t at) { v.objects()[0].type = at; }},
        {[](const nnh::detail::MapView& v, std::uint16_t at) { v.properties()[3].name = at; }},
    };
    for (const Case& c : cases) {
        Bytes file = map_file::sample();
        c.damage(view(file), static_cast<std::uint16_t>(view(file).header().string_bytes));
        CHECK(rejects(file, "string"));
    }
}

NNH_TEST(map_validate_checks_each_tileset_a_layer_uses) {
    // Ids 1..8 are tileset 0, 9..10 tileset 1; id 9 is the first of tileset 1.
    Bytes file = map_file::two_tilesets({9, 10, 0, 0}, 0x02);
    CHECK(accepts(file));
    file = map_file::two_tilesets({9, 0, 0, 0}, 0x01);
    CHECK(rejects(file, "tilesets mask"));
    file = map_file::two_tilesets({8, 0, 0, 0}, 0x02);
    CHECK(rejects(file, "tilesets mask"));
    file = map_file::two_tilesets({8, 9, 0, 0}, 0x03);
    CHECK(accepts(file));
}

NNH_TEST(map_validate_refuses_layer_cells_at_another_offset) {
    Bytes file = map_file::sample();
    view(file).tile_layers()[1].cells -= 2;
    CHECK(rejects(file, "cells"));
}

NNH_TEST(map_validate_refuses_cells_with_unknown_tile_ids) {
    Bytes file = map_file::sample();
    view(file).cells(1)[31] = fmt::encode_cell(9, false, false, false); // the very last cell
    CHECK(rejects(file, "tile id"));
    view(file).cells(1)[31] = fmt::encode_cell(8, true, true, false);
    CHECK(accepts(file));
}

NNH_TEST(map_validate_refuses_diagonal_flips_on_non_square_tiles) {
    Bytes file = map_file::sample();
    fmt::Header& h = view(file).header();
    h.tile_height = 32; // the tileset (64 x 32) still holds 4 x 1 tiles... with 8 declared: fix the tileset
    view(file).tilesets()[0].image_height = 64;
    for (int i = 0; i < 8; ++i) view(file).tile_sources()[1 + i].y = static_cast<std::uint16_t>(i / 4 * 32);
    CHECK(rejects(file, "diagonal"));
    view(file).cells(0)[8 + 2] = fmt::encode_cell(5, true, false, false);
    CHECK(accepts(file));
}

NNH_TEST(map_validate_refuses_a_layer_that_hides_a_tileset_it_uses) {
    Bytes file = map_file::sample();
    view(file).tile_layers()[0].tilesets_used = 0;
    CHECK(rejects(file, "tilesets"));
    file = map_file::sample();
    view(file).tile_layers()[0].tilesets_used = 0x03; // tileset 1 does not exist
    CHECK(rejects(file, "tilesets"));
}

NNH_TEST(map_validate_ignores_the_runtime_fields) {
    Bytes file = map_file::sample();
    view(file).header().collision_layers = 0xFFFF;
    view(file).tilesets()[0].texture_slot = 1234;
    view(file).tilesets()[0].texture_generation = 77;
    CHECK(accepts(file));
}

// --- objects ---

NNH_TEST(map_validate_refuses_object_layer_counts_that_do_not_add_up) {
    Bytes file = map_file::sample();
    view(file).object_layers()[0].object_count = 1;
    CHECK(rejects(file, "object"));
}

NNH_TEST(map_validate_refuses_objects_on_unknown_object_layers) {
    Bytes file = map_file::sample();
    view(file).objects()[1].object_layer = 1;
    CHECK(rejects(file, "unknown object layer"));
}

NNH_TEST(map_validate_refuses_unknown_object_shapes) {
    Bytes file = map_file::sample();
    view(file).objects()[1].shape = 2;
    CHECK(rejects(file, "shape"));
}

NNH_TEST(map_validate_refuses_points_with_a_size) {
    Bytes file = map_file::sample();
    view(file).objects()[0].w = 1;
    CHECK(rejects(file, "point"));
    file = map_file::sample();
    view(file).objects()[0].h = 1;
    CHECK(rejects(file, "point"));
}

NNH_TEST(map_validate_refuses_object_bounds_that_are_negative_or_not_finite) {
    for (int field = 0; field < 4; ++field) {
        Bytes file = map_file::sample();
        fmt::ObjectEntry& o = view(file).objects()[1];
        float* values[] = {&o.x, &o.y, &o.w, &o.h};
        *values[field] = kNaN;
        CHECK(rejects(file, "object bounds"));
        *values[field] = field < 2 ? kInf : -1.0f;
        CHECK(rejects(file, "object bounds"));
    }
}

NNH_TEST(map_validate_refuses_object_ids_outside_1_to_int_max) {
    Bytes file = map_file::sample();
    view(file).objects()[0].id = 0;
    CHECK(rejects(file, "object id"));
    view(file).objects()[0].id = 0x80000000u;
    CHECK(rejects(file, "object id"));
    view(file).objects()[0].id = 0x7FFFFFFFu;
    CHECK(accepts(file));
}

// --- properties ---

NNH_TEST(map_validate_refuses_unknown_property_types) {
    Bytes file = map_file::sample();
    view(file).properties()[0].type = 0; // None
    CHECK(rejects(file, "property type"));
    view(file).properties()[0].type = 5;
    CHECK(rejects(file, "property type"));
}

NNH_TEST(map_validate_refuses_properties_without_a_name) {
    Bytes file = map_file::sample();
    view(file).properties()[2].name = 0;
    CHECK(rejects(file, "name"));
}

NNH_TEST(map_validate_refuses_bools_other_than_0_and_1) {
    Bytes file = map_file::sample();
    const int active = view(file).object_layers()[0].properties_first;
    view(file).properties()[active].value = 2;
    CHECK(rejects(file, "bool"));
}

NNH_TEST(map_validate_refuses_string_values_outside_the_pool) {
    Bytes file = map_file::sample();
    const int music = view(file).header().properties_first;
    view(file).properties()[music].value = view(file).header().string_bytes;
    CHECK(rejects(file, "string"));
    view(file).properties()[music].value = 0x10000u + 1; // would read as offset 1 in 16 bits
    CHECK(rejects(file, "string"));
}

NNH_TEST(map_validate_accepts_any_int_and_float_bits) {
    Bytes file = map_file::sample();
    const int value = view(file).objects()[1].properties_first;
    view(file).properties()[value].value = 0xFFFFFFFFu;
    const int depth = view(file).tile_layers()[0].properties_first;
    view(file).properties()[depth].value = 0x7FC00000u; // NaN, as set_float allows
    CHECK(accepts(file));
}

NNH_TEST(map_validate_refuses_property_ranges_outside_the_properties) {
    struct Case {
        void (*damage)(const nnh::detail::MapView&);
    };
    const Case cases[] = {
        {[](const nnh::detail::MapView& v) { v.header().properties_first = 5; }},
        {[](const nnh::detail::MapView& v) { v.tile_layers()[0].properties_count = 5; }},
        {[](const nnh::detail::MapView& v) { v.object_layers()[0].properties_first = 0xFFFF; }},
        {[](const nnh::detail::MapView& v) { v.objects()[1].properties_count = 2; }},
        {[](const nnh::detail::MapView& v) { v.tile_props()[0].properties_first = 4; }},
    };
    for (const Case& c : cases) {
        Bytes file = map_file::sample();
        c.damage(view(file));
        CHECK(rejects(file, "propert"));
    }
    Bytes file = map_file::sample();
    view(file).header().properties_first = 5; // one past the end: [5, 6)
    CHECK(rejects(file, "outside"));
}

NNH_TEST(map_validate_refuses_overlapping_and_unowned_properties) {
    Bytes file = map_file::sample();
    nnh::detail::MapView v = view(file);
    v.objects()[1].properties_first = v.header().properties_first; // shares the map's "music"
    CHECK(rejects(file, "overlap"));
    file = map_file::sample();
    v = view(file);
    v.objects()[1].properties_count = 0; // "value" belongs to nobody now
    CHECK(rejects(file, "without an owner"));
}

NNH_TEST(map_validate_ignores_the_start_of_empty_ranges) {
    Bytes file = map_file::sample();
    view(file).objects()[0].properties_first = 0xFFFF; // "spawn" has no properties
    CHECK(accepts(file));
}

NNH_TEST(map_validate_refuses_an_unsorted_tile_property_table) {
    // Two tiles with properties, written in the wrong order.
    Bytes file = map_file::minimal();
    const nnh::detail::MapCapacities caps{.width = 1, .height = 1, .tilesets = 1, .tile_ids = 8, .tile_props = 2,
                                          .properties = 2, .string_bytes = 4};
    file.assign(nnh::detail::map_layout(caps).total, 0);
    nnh::detail::init_map_block(file.data(), caps, 16, 16);
    const nnh::detail::MapView v = view(file);
    v.tilesets()[0] = {.first_id = 1, .tile_count = 8, .columns = 4, .image_width = 64, .image_height = 32};
    for (int i = 0; i < 8; ++i) {
        v.tile_sources()[1 + i] = {static_cast<std::uint16_t>(i % 4 * 16), static_cast<std::uint16_t>(i / 4 * 16)};
    }
    fmt::Header& h = v.header();
    h.tilesets = 1;
    h.tile_ids = 8;
    std::memcpy(v.strings(), "\0a\0", 3);
    h.string_bytes = 3;
    v.properties()[0] = {.name = 1, .type = 2, .value = 1};
    v.properties()[1] = {.name = 1, .type = 2, .value = 2};
    h.properties = 2;
    v.tile_props()[0] = {6, 0, 1};
    v.tile_props()[1] = {2, 1, 1};
    h.tile_props = 2;
    v.tile_flags()[2] = v.tile_flags()[6] = fmt::kTileHasProperties;
    CHECK(rejects(file, "sorted"));
    v.tile_props()[0].tile_id = 2;
    v.tile_props()[1].tile_id = 6;
    CHECK(accepts(file));
    v.tile_props()[1].tile_id = 2; // the same tile twice
    v.tile_flags()[6] = 0;
    CHECK(rejects(file, "sorted"));
}
