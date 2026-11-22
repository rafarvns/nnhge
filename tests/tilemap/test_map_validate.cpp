// Validation of `.nnhmap` files (spec 012 FR-1, FR-11, constitution X): header, CRC, limits, layout, strings,
// tilesets and the tile table. Layers, cells, objects and properties: test_map_validate_content.cpp.

#include <cstring>

#include "map_file.hpp"
#include "test.hpp"
#include "tilemap/map_validate.hpp"

using map_file::Bytes;
using nnh::detail::MapProblem;
using nnh::detail::validate_map;
namespace fmt = nnh::detail::mapfmt;

namespace {

bool says(const nnh::detail::MapCheck& c, MapProblem problem, const char* words) {
    return c.problem == problem && std::strstr(c.reason, words) != nullptr;
}

/// Seals the damaged file (so size and CRC are right again) and checks the validator's verdict.
bool rejects(Bytes& file, MapProblem problem, const char* words) {
    map_file::seal(file);
    return says(validate_map(file.data(), file.size()), problem, words);
}

fmt::Header& header(Bytes& file) { return map_file::view(file).header(); }

} // namespace

NNH_TEST(map_validate_accepts_the_sample_and_the_minimal_map) {
    Bytes sample = map_file::sample();
    Bytes minimal = map_file::minimal();
    CHECK(validate_map(sample.data(), sample.size()).ok());
    CHECK(validate_map(minimal.data(), minimal.size()).ok());
}

NNH_TEST(map_validate_accepts_any_alignment) {
    const Bytes sample = map_file::sample();
    Bytes shifted(sample.size() + 1);
    std::memcpy(shifted.data() + 1, sample.data(), sample.size());
    CHECK(validate_map(shifted.data() + 1, sample.size()).ok());
}

NNH_TEST(map_validate_refuses_files_shorter_than_a_header) {
    const Bytes sample = map_file::sample();
    CHECK(says(validate_map(nullptr, 0), MapProblem::Damaged, "too short"));
    CHECK(says(validate_map(sample.data(), 127), MapProblem::Damaged, "too short"));
}

NNH_TEST(map_validate_refuses_another_magic) {
    Bytes file = map_file::sample();
    header(file).magic[3] = 'X';
    CHECK(rejects(file, MapProblem::Damaged, "not a map"));
}

NNH_TEST(map_validate_refuses_newer_versions_as_unsupported) {
    Bytes file = map_file::sample();
    header(file).version = 2;
    CHECK(rejects(file, MapProblem::NewerVersion, "newer"));
    header(file).version = 0;
    CHECK(rejects(file, MapProblem::Damaged, "version"));
}

NNH_TEST(map_validate_refuses_truncated_and_padded_files) {
    Bytes file = map_file::sample();
    CHECK(says(validate_map(file.data(), file.size() - 4), MapProblem::Damaged, "size"));
    file.push_back(0);
    CHECK(says(validate_map(file.data(), file.size()), MapProblem::Damaged, "size"));
}

NNH_TEST(map_validate_refuses_a_crc_mismatch) {
    Bytes file = map_file::sample();
    file.back() ^= 1;
    CHECK(says(validate_map(file.data(), file.size()), MapProblem::Damaged, "CRC"));
    file = map_file::sample();
    file[16] ^= 1; // the first byte the CRC covers
    CHECK(says(validate_map(file.data(), file.size()), MapProblem::Damaged, "CRC"));
}

NNH_TEST(map_validate_refuses_capacities_over_the_limits) {
    Bytes file = map_file::sample();
    header(file).tile_layers_cap = 17;
    CHECK(rejects(file, MapProblem::OverLimit, "16 tile layers"));
    file = map_file::sample();
    header(file).width = 1025;
    CHECK(rejects(file, MapProblem::OverLimit, "1024"));
}

NNH_TEST(map_validate_refuses_an_empty_map_size) {
    Bytes file = map_file::sample();
    header(file).height = 0;
    CHECK(rejects(file, MapProblem::Damaged, "1x1"));
}

NNH_TEST(map_validate_refuses_tile_sizes_outside_8_to_64) {
    Bytes file = map_file::sample();
    header(file).tile_width = 7;
    CHECK(rejects(file, MapProblem::OverLimit, "tile size"));
    file = map_file::sample();
    header(file).tile_height = 65;
    CHECK(rejects(file, MapProblem::OverLimit, "tile size"));
    file = map_file::minimal();
    header(file).tile_width = header(file).tile_height = 64;
    CHECK(validate_map((map_file::seal(file), file.data()), file.size()).ok());
}

NNH_TEST(map_validate_refuses_a_block_size_other_than_the_capacities_need) {
    Bytes file = map_file::sample();
    header(file).objects_cap = 3; // one more object would need 36 more bytes
    CHECK(rejects(file, MapProblem::Damaged, "capacities"));
}

NNH_TEST(map_validate_refuses_a_last_section_that_runs_past_the_file) {
    // The strings come last: a bigger string capacity moves no offset, only the end of the block.
    Bytes file = map_file::sample();
    header(file).string_cap += 4;
    CHECK(rejects(file, MapProblem::Damaged, "block size"));
}

NNH_TEST(map_validate_refuses_moved_sections) {
    Bytes file = map_file::sample();
    header(file).offsets[fmt::kStrings] += 4;
    CHECK(rejects(file, MapProblem::Damaged, "offsets"));
    file = map_file::sample();
    header(file).offsets[fmt::kTilesets] -= 4;
    CHECK(rejects(file, MapProblem::Damaged, "offsets"));
}

NNH_TEST(map_validate_refuses_counts_over_capacities) {
    struct Case {
        void (*damage)(fmt::Header&);
    };
    const Case cases[] = {
        {[](fmt::Header& h) { h.tilesets = 2; }},       {[](fmt::Header& h) { h.tile_layers = 3; }},
        {[](fmt::Header& h) { h.object_layers = 2; }},  {[](fmt::Header& h) { h.tile_ids = 9; }},
        {[](fmt::Header& h) { h.tile_props = 2; }},     {[](fmt::Header& h) { h.objects = 3; }},
        {[](fmt::Header& h) { h.properties = 6; }},     {[](fmt::Header& h) { h.animations = 1; }},
        {[](fmt::Header& h) { h.frames = 1; }},         {[](fmt::Header& h) { h.string_bytes = 129; }},
    };
    for (const Case& c : cases) {
        Bytes file = map_file::sample();
        c.damage(header(file));
        CHECK(rejects(file, MapProblem::Damaged, "capacity"));
    }
}

NNH_TEST(map_validate_accepts_room_for_animations_that_is_not_used) {
    // Animated tiles themselves: test_map_validate_animations.cpp.
    const nnh::detail::MapCapacities caps{.width = 1, .height = 1, .animations = 1, .animation_frames = 2,
                                          .string_bytes = 1};
    Bytes table(nnh::detail::map_layout(caps).total);
    nnh::detail::init_map_block(table.data(), caps, 16, 16);
    CHECK(validate_map((map_file::seal(table), table.data()), table.size()).ok());
}

// --- strings ---

NNH_TEST(map_validate_refuses_a_pool_without_the_empty_string) {
    Bytes file = map_file::sample();
    map_file::view(file).strings()[0] = 'x';
    CHECK(rejects(file, MapProblem::Damaged, "empty string"));
    file = map_file::sample();
    header(file).string_bytes = 0;
    CHECK(rejects(file, MapProblem::Damaged, "empty string"));
}

NNH_TEST(map_validate_refuses_an_unterminated_last_string) {
    Bytes file = map_file::sample();
    const std::uint32_t used = header(file).string_bytes;
    map_file::view(file).strings()[used - 1] = 'x';
    CHECK(rejects(file, MapProblem::Damaged, "terminated"));
}

NNH_TEST(map_validate_refuses_strings_over_255_bytes) {
    Bytes file = map_file::minimal();
    // Grow the pool to 260 bytes: "" + 256 'a' + NUL.
    const std::uint32_t pool = header(file).offsets[fmt::kStrings];
    file.resize(pool + 260);
    header(file).string_cap = 260;
    header(file).string_bytes = 258;
    std::memset(file.data() + pool + 1, 'a', 256);
    file[pool + 257] = '\0';
    CHECK(rejects(file, MapProblem::Damaged, "255"));
    file[pool + 256] = '\0'; // "" + 255 'a' + NUL + NUL: fine
    CHECK(validate_map((map_file::seal(file), file.data()), file.size()).ok());
}

NNH_TEST(map_validate_refuses_string_offsets_outside_the_used_pool) {
    Bytes file = map_file::sample();
    map_file::view(file).tilesets()[0].name = static_cast<std::uint16_t>(header(file).string_bytes);
    CHECK(rejects(file, MapProblem::Damaged, "string"));
}

// --- tilesets and tiles ---

NNH_TEST(map_validate_refuses_tilesets_whose_ids_do_not_follow_each_other) {
    Bytes file = map_file::sample();
    map_file::view(file).tilesets()[0].first_id = 2;
    CHECK(rejects(file, MapProblem::Damaged, "tileset ids"));
    file = map_file::sample();
    map_file::view(file).tilesets()[0].tile_count = 7; // 7 ids, the header says 8
    CHECK(rejects(file, MapProblem::Damaged, "tileset ids"));
    file = map_file::sample();
    map_file::view(file).tilesets()[0].tile_count = 0;
    CHECK(rejects(file, MapProblem::Damaged, "tileset ids"));
}

NNH_TEST(map_validate_refuses_tilesets_without_tiles) {
    // Tileset "a" claims no ids and "b" starts at id 1: the ids still follow each other, but "a" is empty.
    Bytes file = map_file::two_tilesets({1, 2, 0, 0}, 0x02);
    nnh::detail::MapView v = map_file::view(file);
    v.tilesets()[0].tile_count = 0;
    v.tilesets()[1].first_id = 1;
    v.tilesets()[1].tile_count = 2;
    v.header().tile_ids = 2;
    v.tile_sources()[1] = {0, 0};
    v.tile_sources()[2] = {16, 0};
    CHECK(rejects(file, MapProblem::Damaged, "tileset ids"));
}

NNH_TEST(map_validate_refuses_tileset_images_that_are_not_textures) {
    Bytes file = map_file::sample();
    map_file::view(file).tilesets()[0].image_width = 48;
    CHECK(rejects(file, MapProblem::Damaged, "2^n"));
    file = map_file::sample();
    map_file::view(file).tilesets()[0].image_height = 2048;
    CHECK(rejects(file, MapProblem::Damaged, "2^n"));
}

NNH_TEST(map_validate_refuses_tilesets_that_do_not_fit_their_image) {
    Bytes file = map_file::sample();
    map_file::view(file).tilesets()[0].columns = 3;
    CHECK(rejects(file, MapProblem::Damaged, "columns"));
    file = map_file::sample();
    map_file::view(file).tilesets()[0].margin = 8; // 64 - 16 = 48: 3 columns
    CHECK(rejects(file, MapProblem::Damaged, "columns"));
    file = map_file::sample();
    map_file::view(file).tilesets()[0].spacing = 1; // (64 + 1) / 17 = 3 columns
    CHECK(rejects(file, MapProblem::Damaged, "columns"));
}

NNH_TEST(map_validate_refuses_more_tiles_than_the_image_holds) {
    // 9 tiles in a 64 x 32 image of 16 x 16 tiles (8 fit), with the header and table grown to match.
    const nnh::detail::MapCapacities caps{.width = 1, .height = 1, .tilesets = 1, .tile_ids = 9,
                                          .string_bytes = 1};
    Bytes file(nnh::detail::map_layout(caps).total);
    nnh::detail::init_map_block(file.data(), caps, 16, 16);
    const nnh::detail::MapView v = map_file::view(file);
    v.tilesets()[0] = {.first_id = 1, .tile_count = 9, .columns = 4, .image_width = 64, .image_height = 32};
    for (int i = 0; i < 9; ++i) {
        v.tile_sources()[1 + i] = {static_cast<std::uint16_t>(i % 4 * 16), static_cast<std::uint16_t>(i / 4 * 16)};
    }
    v.header().tilesets = 1;
    v.header().tile_ids = 9;
    CHECK(rejects(file, MapProblem::Damaged, "fit"));
    v.tilesets()[0].image_height = 64; // now 16 fit
    CHECK(validate_map((map_file::seal(file), file.data()), file.size()).ok());
}

NNH_TEST(map_validate_refuses_tile_sources_other_than_the_tileset_gives) {
    Bytes file = map_file::sample();
    map_file::view(file).tile_sources()[6].x += 1;
    CHECK(rejects(file, MapProblem::Damaged, "source"));
    file = map_file::sample();
    map_file::view(file).tile_sources()[8].y = 0; // the last tile
    CHECK(rejects(file, MapProblem::Damaged, "source"));
    file = map_file::sample();
    map_file::view(file).tile_sources()[0].x = 16; // id 0 is empty: no source
    CHECK(rejects(file, MapProblem::Damaged, "source"));
}

NNH_TEST(map_validate_refuses_unknown_tile_flags) {
    Bytes file = map_file::sample();
    map_file::view(file).tile_flags()[1] = 3; // collision value 3 does not exist
    CHECK(rejects(file, MapProblem::Damaged, "tile flags"));
    file = map_file::sample();
    map_file::view(file).tile_flags()[1] = 0x10;
    CHECK(rejects(file, MapProblem::Damaged, "tile flags"));
    file = map_file::sample();
    map_file::view(file).tile_flags()[0] = 1; // id 0 has no flags
    CHECK(rejects(file, MapProblem::Damaged, "tile flags"));
}

NNH_TEST(map_validate_refuses_a_has_properties_flag_without_a_table_entry) {
    Bytes file = map_file::sample();
    map_file::view(file).tile_flags()[4] |= fmt::kTileHasProperties;
    CHECK(rejects(file, MapProblem::Damaged, "tile propert"));
    file = map_file::sample();
    map_file::view(file).tile_flags()[3] &= static_cast<std::uint8_t>(~fmt::kTileHasProperties);
    CHECK(rejects(file, MapProblem::Damaged, "tile propert"));
    file = map_file::sample();
    map_file::view(file).tile_props()[0].tile_id = 4; // as many flags as entries, but on another tile
    CHECK(rejects(file, MapProblem::Damaged, "tile propert"));
}

NNH_TEST(map_validate_refuses_tile_property_entries_of_unknown_tiles) {
    Bytes file = map_file::sample();
    map_file::view(file).tile_props()[0].tile_id = 9;
    CHECK(rejects(file, MapProblem::Damaged, "unknown tile id"));
    file = map_file::sample();
    map_file::view(file).tile_props()[0].tile_id = 0;
    CHECK(rejects(file, MapProblem::Damaged, "unknown tile id"));
}
