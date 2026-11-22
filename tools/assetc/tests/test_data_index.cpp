// The data index step (spec 014 T6.1, T6.2; FR-4, FR-8, FR-9, FR-12, FR-14): names, references, media kinds,
// hashes and limits checked across the whole game, then the `.nnh_data` file the engine reads.

#include <string>
#include <vector>

#include "convert_fixture.hpp"
#include "data/index_validate.hpp"
#include "data_index_build.hpp"
#include "nnh/detail/name_hash.hpp"
#include "test.hpp"

using nnh::assetc::DataReference;
using nnh::assetc::IndexInput;
using nnh::assetc::IndexModel;
using nnh::assetc::RefsFile;
using nnh::detail::FieldKind;

namespace {

RefsFile data(const std::string& name, const std::string& key, std::size_t size = 40,
              std::vector<DataReference> refs = {}) {
    RefsFile f;
    f.data = true;
    f.name = name;
    f.key = key;
    f.size = size;
    f.references = std::move(refs);
    return f;
}

DataReference ref(int line, FieldKind kind, const std::string& path, const std::string& field = "f",
                  const std::string& target = "") {
    DataReference r;
    r.line = line;
    r.kind = kind;
    r.path = path;
    r.field = field;
    r.target = kind == FieldKind::DataRef && target.empty() ? "item" : target;
    return r;
}

const std::vector<std::string> kAssets = {
    "art/goblin.png", "art/potion.png", "enemies/bat.enemy", "enemies/goblin.enemy", "items/potion.item",
    "items/ether.item", "music/battle.ogg", "sfx/hurt.wav", "sfx/hit.ogg", "notes.txt"};

IndexInput input(std::vector<RefsFile> files, std::vector<std::string> assets = kAssets) {
    IndexInput in;
    in.schema = &convert_test::schema();
    in.assets = std::move(assets);
    in.data = std::move(files);
    return in;
}

/// The printed problems of a failing build.
std::string errors_of(const IndexInput& in) {
    IndexModel model;
    std::string errors;
    CHECK(!nnh::assetc::build_index(in, model, errors));
    return errors;
}

std::string goblin_with(DataReference r) {
    return errors_of(input({data("enemies/goblin", "enemy", 40, {r}), data("items/potion", "item"),
                            data("items/ether", "item"), data("enemies/bat", "enemy")}));
}

std::uint32_t same_hash(std::string_view) { return 7; }
std::uint32_t zero_for_bat(std::string_view p) { return p == "enemies/bat" ? 0 : nnh::assetc::fnv1a_path(p); }

} // namespace

NNH_TEST(data_index_lists_every_keyed_type_by_key_with_its_assets_in_path_order) {
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(
        input({data("items/potion", "item", 30), data("enemies/goblin", "enemy", 90), data("items/ether", "item")}),
        m, errors));
    CHECK(errors.empty() && m.types.size() == 2);
    CHECK(m.types[0].key == "enemy" && m.types[1].key == "item");
    CHECK(m.types[0].fingerprint == convert_test::schema().fingerprint(*convert_test::schema().type_by_key("enemy")));
    CHECK(m.types[0].paths == std::vector<std::string>{"enemies/goblin"});
    CHECK((m.types[1].paths == std::vector<std::string>{"items/ether", "items/potion"}));
    CHECK(m.largest == 90);
}

NNH_TEST(data_index_lists_a_type_without_assets) {
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(input({data("items/potion", "item")}), m, errors));
    CHECK(m.types.size() == 2 && m.types[0].key == "enemy" && m.types[0].paths.empty());
}

NNH_TEST(data_index_lists_only_the_media_data_assets_reference_once_each) {
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(
        input({data("enemies/goblin", "enemy", 40,
                    {ref(2, FieldKind::TextureRef, "art/goblin"), ref(3, FieldKind::SoundRef, "sfx/hurt"),
                     ref(4, FieldKind::SoundRef, "sfx/hit"), ref(5, FieldKind::MusicRef, "music/battle")}),
               data("enemies/bat", "enemy", 40, {ref(2, FieldKind::TextureRef, "art/goblin")})}),
        m, errors));
    CHECK(errors.empty());
    CHECK(m.media[0] == std::vector<std::string>{"art/goblin"});
    CHECK((m.media[1] == std::vector<std::string>{"sfx/hit", "sfx/hurt"}));
    CHECK(m.media[2] == std::vector<std::string>{"music/battle"});
}

NNH_TEST(data_index_accepts_references_between_data_assets_and_cycles) {
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(
        input({data("enemies/goblin", "enemy", 40, {ref(5, FieldKind::DataRef, "items/potion", "drops[0].item")}),
               data("items/potion", "item", 40, {ref(3, FieldKind::DataRef, "enemies/goblin", "summons", "enemy")})}),
        m, errors));
    CHECK(errors.empty());
}

NNH_TEST(data_index_refuses_a_name_used_by_two_types) {
    CHECK(errors_of(input({data("items/potion", "item"), data("items/potion", "enemy")})) ==
          "assets/items/potion.item:1: the data asset name 'items/potion' is also used by "
          "assets/items/potion.enemy: names must be unique across types\n");
}

NNH_TEST(data_index_refuses_a_missing_data_reference_and_suggests_one_of_its_type) {
    CHECK(goblin_with(ref(9, FieldKind::DataRef, "items/potoin", "drop")) ==
          "assets/enemies/goblin.enemy:9: drop: no item 'items/potoin' (no file assets/items/potoin.item; did "
          "you mean 'items/potion'?)\n");
    CHECK(goblin_with(ref(9, FieldKind::DataRef, "loot/sword", "drop")) ==
          "assets/enemies/goblin.enemy:9: drop: no item 'loot/sword' (no file assets/loot/sword.item)\n");
    CHECK(goblin_with(ref(4, FieldKind::DataRef, "enemies/bta", "summons", "enemy")) ==
          "assets/enemies/goblin.enemy:4: summons: no enemy 'enemies/bta' (no file assets/enemies/bta.enemy; "
          "did you mean 'enemies/bat'?)\n");
}

NNH_TEST(data_index_refuses_a_data_reference_of_the_wrong_kind) {
    CHECK(goblin_with(ref(9, FieldKind::DataRef, "enemies/bat", "drops[1].item")) ==
          "assets/enemies/goblin.enemy:9: drops[1].item: 'enemies/bat' is an enemy, not an item\n");
    CHECK(goblin_with(ref(9, FieldKind::DataRef, "art/potion", "drop")) ==
          "assets/enemies/goblin.enemy:9: drop: 'art/potion' is an image, not an item\n");
    CHECK(goblin_with(ref(9, FieldKind::DataRef, "sfx/hurt", "drop")) ==
          "assets/enemies/goblin.enemy:9: drop: 'sfx/hurt' is a sound, not an item\n");
}

NNH_TEST(data_index_refuses_a_texture_reference_to_anything_but_an_image) {
    CHECK(goblin_with(ref(3, FieldKind::TextureRef, "sfx/hurt", "sprite")) ==
          "assets/enemies/goblin.enemy:3: sprite: 'sfx/hurt' is a sound, not a texture\n");
    CHECK(goblin_with(ref(3, FieldKind::TextureRef, "items/potion", "sprite")) ==
          "assets/enemies/goblin.enemy:3: sprite: 'items/potion' is an item, not a texture\n");
    CHECK(goblin_with(ref(3, FieldKind::TextureRef, "notes", "sprite")) ==
          "assets/enemies/goblin.enemy:3: sprite: no texture 'notes' (no file assets/notes.png)\n");
    CHECK(goblin_with(ref(3, FieldKind::TextureRef, "art/gobiln", "sprite")) ==
          "assets/enemies/goblin.enemy:3: sprite: no texture 'art/gobiln' (no file assets/art/gobiln.png; did "
          "you mean 'art/goblin'?)\n");
}

NNH_TEST(data_index_refuses_a_sound_or_music_reference_to_anything_but_audio) {
    CHECK(goblin_with(ref(6, FieldKind::SoundRef, "art/goblin", "hurt")) ==
          "assets/enemies/goblin.enemy:6: hurt: 'art/goblin' is an image, not a sound\n");
    CHECK(goblin_with(ref(6, FieldKind::MusicRef, "enemies/bat", "theme")) ==
          "assets/enemies/goblin.enemy:6: theme: 'enemies/bat' is an enemy, not music\n");
    CHECK(goblin_with(ref(6, FieldKind::SoundRef, "sfx/hurts", "hurt")) ==
          "assets/enemies/goblin.enemy:6: hurt: no sound 'sfx/hurts' (no file assets/sfx/hurts.wav or .ogg; did "
          "you mean 'sfx/hurt'?)\n");
    CHECK(goblin_with(ref(6, FieldKind::MusicRef, "music/boss", "theme")) ==
          "assets/enemies/goblin.enemy:6: theme: no music 'music/boss' (no file assets/music/boss.wav or .ogg)\n");
}

NNH_TEST(data_index_accepts_wav_or_ogg_for_sounds_and_music) {
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(input({data("enemies/goblin", "enemy", 40,
                                               {ref(2, FieldKind::MusicRef, "sfx/hurt"),
                                                ref(3, FieldKind::SoundRef, "music/battle")})}),
                                   m, errors));
    CHECK(errors.empty() && m.media[1] == std::vector<std::string>{"music/battle"});
}

NNH_TEST(data_index_reports_only_the_first_file_with_problems_sorted_by_line) {
    CHECK(errors_of(input({data("items/b", "item", 40, {ref(3, FieldKind::TextureRef, "x")}),
                           data("items/a", "item", 40,
                                {ref(5, FieldKind::TextureRef, "y"), ref(2, FieldKind::TextureRef, "z")})})) ==
          "assets/items/a.item:2: f: no texture 'z' (no file assets/z.png)\n"
          "assets/items/a.item:5: f: no texture 'y' (no file assets/y.png)\n");
}

NNH_TEST(data_index_prints_at_most_20_problems_of_a_file) {
    std::vector<DataReference> refs;
    for (int i = 1; i <= 22; ++i)
        refs.push_back(ref(i, FieldKind::TextureRef, "missing"));
    const std::string errors = errors_of(input({data("items/a", "item", 40, refs)}));
    CHECK(errors.find("assets/items/a.item:20: ") != std::string::npos);
    CHECK(errors.find("assets/items/a.item:21: ") == std::string::npos);
    CHECK(errors.find("assets/items/a.item: ... and 2 more\n") != std::string::npos);
}

NNH_TEST(data_index_shows_paths_under_the_given_folder) {
    IndexInput in = input({data("items/a", "item", 40, {ref(2, FieldKind::TextureRef, "x")})});
    in.shown = "game/assets";
    CHECK(errors_of(in) == "game/assets/items/a.item:2: f: no texture 'x' (no file game/assets/x.png)\n");
}

NNH_TEST(data_index_refuses_a_sidecar_of_a_type_the_schema_doesnt_have) {
    CHECK(errors_of(input({data("things/a", "thing")})) ==
          "assets/things/a.thing:1: the schema has no data type 'thing': rebuild the assets\n");
}

NNH_TEST(data_index_refuses_two_data_names_with_one_hash) {
    IndexInput in = input({data("enemies/goblin", "enemy"), data("enemies/bat", "enemy")});
    in.hash = same_hash;
    CHECK(errors_of(in) == "assets/enemies/goblin.enemy:1: the names 'enemies/bat' and 'enemies/goblin' have the "
                           "same hash 0x00000007: rename one of them\n");
}

NNH_TEST(data_index_refuses_a_data_name_whose_hash_is_0) {
    IndexInput in = input({data("enemies/goblin", "enemy"), data("enemies/bat", "enemy")});
    in.hash = zero_for_bat;
    CHECK(errors_of(in) == "assets/enemies/bat.enemy:1: the name 'enemies/bat' hashes to 0: rename the asset\n");
}

NNH_TEST(data_index_refuses_two_media_paths_with_one_hash_or_a_hash_of_0) {
    IndexInput in = input({data("enemies/goblin", "enemy", 40,
                                {ref(2, FieldKind::TextureRef, "art/goblin"), ref(3, FieldKind::TextureRef, "art/potion")})});
    in.hash = [](std::string_view p) { return p.substr(0, 4) == "art/" ? 9u : nnh::assetc::fnv1a_path(p); };
    CHECK(errors_of(in) == "assets: the paths 'art/goblin' and 'art/potion' have the same hash 0x00000009: rename "
                           "one of them\n");
    in.hash = [](std::string_view p) { return p == "art/potion" ? 0u : nnh::assetc::fnv1a_path(p); };
    CHECK(errors_of(in) == "assets: the path 'art/potion' hashes to 0: rename the asset\n");
}

NNH_TEST(data_index_hashes_media_groups_separately) {
    IndexInput in = input({data("enemies/goblin", "enemy", 40,
                                {ref(2, FieldKind::TextureRef, "art/goblin"), ref(3, FieldKind::SoundRef, "sfx/hurt")})});
    in.hash = [](std::string_view p) { return p == "enemies/goblin" ? 1u : 5u; };
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(in, m, errors) && errors.empty());
}

NNH_TEST(data_index_accepts_4096_data_assets_and_refuses_4097) {
    std::vector<RefsFile> files;
    for (int i = 0; i < 4096; ++i)
        files.push_back(data("e/" + std::to_string(i), "enemy"));
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(input(files), m, errors));
    files.push_back(data("e/x", "enemy"));
    CHECK(errors_of(input(files)) == "assets: 4097 data assets: at most 4096\n");
}

NNH_TEST(data_index_refuses_more_than_4096_referenced_media) {
    std::vector<DataReference> refs;
    std::vector<std::string> assets;
    for (int i = 0; i < 4097; ++i) {
        refs.push_back(ref(1, FieldKind::TextureRef, "t/" + std::to_string(i)));
        assets.push_back("t/" + std::to_string(i) + ".png");
    }
    CHECK(errors_of(input({data("items/a", "item", 40, refs)}, assets)) ==
          "assets: 4097 textures, sounds and music referenced by data assets: at most 4096\n");
}

NNH_TEST(data_index_refuses_paths_over_256_kib) {
    std::vector<RefsFile> files;
    const std::string long_name(200, 'n');
    for (int i = 0; i < 1300; ++i) // 1300 x (4 + 1 + 200 + 1) bytes > 256 KiB
        files.push_back(data(std::to_string(1000 + i) + "/" + long_name, "item"));
    CHECK(errors_of(input(files)) == "assets: the data index's paths take 267800 bytes: at most 262144\n");
}

NNH_TEST(data_index_file_passes_the_engines_validator_and_finds_every_path) {
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(
        input({data("enemies/goblin", "enemy", 77, {ref(2, FieldKind::TextureRef, "art/goblin"),
                                                    ref(3, FieldKind::SoundRef, "sfx/hurt"),
                                                    ref(4, FieldKind::MusicRef, "music/battle")}),
               data("items/potion", "item"), data("items/ether", "item")}),
        m, errors));
    std::string problem;
    const std::vector<std::uint8_t> file = nnh::assetc::write_index(m, problem);
    CHECK(problem.empty() && !file.empty());
    CHECK(nnh::detail::validate_index(file.data(), file.size()).ok());
    const nnh::detail::IndexView view(file.data());
    CHECK(view.type_count() == 2 && view.largest() == 77);
    CHECK(view.type(view.find_type("item")).count == 2);
    using G = nnh::detail::idxfmt::Group;
    const int ether = view.find(G::Data, nnh::detail::fnv1a32("items/ether"));
    CHECK(ether >= 0 && std::string(view.path(ether)) == "items/ether");
    CHECK(view.type_of(ether) == view.find_type("item"));
    CHECK(std::string(view.path(view.find(G::Texture, nnh::detail::fnv1a32("art/goblin")))) == "art/goblin");
    CHECK(std::string(view.path(view.find(G::Sound, nnh::detail::fnv1a32("sfx/hurt")))) == "sfx/hurt");
    CHECK(std::string(view.path(view.find(G::Music, nnh::detail::fnv1a32("music/battle")))) == "music/battle");
    CHECK(view.find(G::Sound, nnh::detail::fnv1a32("art/goblin")) == -1);
}

NNH_TEST(data_index_accepts_media_with_one_letter_names) {
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(input({data("items/a", "item", 40, {ref(2, FieldKind::TextureRef, "x"),
                                                                        ref(3, FieldKind::SoundRef, "y")})},
                                         {"x.png", "y.ogg"}),
                                   m, errors));
    CHECK(errors.empty() && m.media[0].size() == 1 && m.media[1].size() == 1);
}

NNH_TEST(data_index_refuses_a_reference_to_a_type_the_schema_doesnt_have) {
    CHECK(goblin_with(ref(5, FieldKind::DataRef, "items/potion", "loot", "thing")) ==
          "assets/enemies/goblin.enemy:5: loot: the schema has no data type 'thing'\n");
}

NNH_TEST(data_index_ignores_sidecars_of_files_that_arent_data_assets) {
    RefsFile copied; // a copied asset's sidecar has no 'asset' line
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(input({copied, data("items/potion", "item")}), m, errors) && errors.empty());
    CHECK(m.types[1].paths == std::vector<std::string>{"items/potion"});
}

NNH_TEST(data_index_file_with_many_entries_is_sorted_for_lookups) {
    std::vector<RefsFile> files;
    for (int i = 0; i < 60; ++i)
        files.push_back(data("items/i" + std::to_string(i), "item"));
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(input(files), m, errors));
    std::string problem;
    const std::vector<std::uint8_t> file = nnh::assetc::write_index(m, problem);
    CHECK(problem.empty() && nnh::detail::validate_index(file.data(), file.size()).ok());
    const nnh::detail::IndexView view(file.data());
    for (int i = 0; i < 60; ++i) {
        const std::string p = "items/i" + std::to_string(i);
        const int e = view.find(nnh::detail::idxfmt::Group::Data, nnh::detail::fnv1a32(p.c_str()));
        CHECK(e >= 0 && p == view.path(e));
    }
}

NNH_TEST(data_index_file_of_a_game_without_data_assets_is_valid) {
    IndexModel m;
    std::string errors;
    CHECK(nnh::assetc::build_index(input({}), m, errors));
    std::string problem;
    const std::vector<std::uint8_t> file = nnh::assetc::write_index(m, problem);
    CHECK(nnh::detail::validate_index(file.data(), file.size()).ok());
    CHECK(nnh::detail::IndexView(file.data()).entry_count() == 0);
}
