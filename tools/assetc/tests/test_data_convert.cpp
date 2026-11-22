// Converting one data file's values (spec 014 T5.2, FR-2, FR-5, FR-7, FR-8, FR-9): every kind, defaults,
// ranges, strings, enums, references; each problem as `file:line: reason`. The converted bytes pass the
// engine's validator and load into the compiled struct (tests/data_assets/test_types.hpp).

#include <cstring>
#include <string>

#include "convert_fixture.hpp"
#include "nnh/detail/name_hash.hpp"
#include "test.hpp"
#include "test_types.hpp"

using convert_test::convert;
using convert_test::load_compiled;
using nnh::detail::fnv1a32;
using test_data::Enemy;
using test_data::Item;

namespace {

const char kGoblin[] = "# a goblin\n"
                       "name = \"Goblin\"\n"
                       "speed = 1.5\n"
                       "element = \"Fire\"\n"
                       "sprite = \"art/goblin\"\n"
                       "hurt = \"sfx/hurt\"\n"
                       "tint = \"#0A141E28\"\n"
                       "spawn = [3, -4.5]\n"
                       "flags = [true, false, true]\n"
                       "level = 7\n"
                       "theme = \"\"\n"
                       "\n"
                       "[stats]\n"
                       "hp = 30\n"
                       "attack = -3\n"
                       "\n"
                       "[[drops]]\n"
                       "item = \"items/potion\"\n"
                       "chance = 0.25\n";

/// The problems of `text` (an enemy that sets `name`, plus `text`).
std::string problems(const std::string& text, const char* key = "enemy") {
    const std::string full = std::strcmp(key, "enemy") == 0 ? "name = \"x\"\n" + text : text;
    const auto r = convert(full, key);
    return r.ok ? "ok" : r.problems;
}

} // namespace

NNH_TEST(data_convert_writes_every_value_of_the_file) {
    const auto r = convert(kGoblin);
    CHECK(r.ok && r.problems.empty());
    Enemy e;
    CHECK(load_compiled(r.data.bytes, e));
    CHECK(e.name == "Goblin" && e.speed == 1.5f && e.element == test_data::Element::Fire);
    CHECK(e.sprite.id == fnv1a32("art/goblin") && e.hurt.id == fnv1a32("sfx/hurt") && e.theme.empty());
    CHECK(e.tint.r == 10 && e.tint.g == 20 && e.tint.b == 30 && e.tint.a == 40);
    CHECK(e.spawn.x == 3 && e.spawn.y == -4.5f);
    CHECK(e.flags[0] && !e.flags[1] && e.flags[2] && e.level == 7);
    CHECK(e.stats.hp == 30 && e.stats.attack == -3);
    CHECK(e.drops.size() == 1 && e.drops.begin()[0].item.id == fnv1a32("items/potion") &&
          e.drops.begin()[0].chance == 0.25f);
}

NNH_TEST(data_convert_collects_the_references_with_their_lines) {
    const auto r = convert(kGoblin);
    const auto& refs = r.data.references;
    CHECK(refs.size() == 3); // the empty theme is not a reference
    CHECK(refs[0].line == 5 && refs[0].kind == nnh::detail::FieldKind::TextureRef &&
          refs[0].path == "art/goblin");
    CHECK(refs[0].field == "sprite" && refs[0].target.empty());
    CHECK(refs[1].line == 6 && refs[1].kind == nnh::detail::FieldKind::SoundRef);
    CHECK(refs[2].line == 18 && refs[2].kind == nnh::detail::FieldKind::DataRef && refs[2].target == "item");
    CHECK(refs[2].field == "drops[0].item" && refs[2].path == "items/potion");
}

NNH_TEST(data_convert_leaves_unset_fields_to_the_structs_defaults) {
    const auto r = convert("name = \"Bat\"\n");
    CHECK(r.ok);
    Enemy e;
    CHECK(load_compiled(r.data.bytes, e));
    const Enemy defaults;
    CHECK(e.name == "Bat" && e.stats.hp == defaults.stats.hp && e.speed == defaults.speed && e.level == 1);
    CHECK(e.spawn.x == 8 && e.spawn.y == 16 && e.flags[1] && e.drops.empty());
    // The body is the one field: count 1, field 0, "Bat".
    const std::vector<std::uint8_t> body(r.data.bytes.begin() + 20, r.data.bytes.end());
    CHECK((body == std::vector<std::uint8_t>{1, 0, 3, 'B', 'a', 't'}));
}

NNH_TEST(data_convert_reports_unknown_fields_with_a_suggestion) {
    CHECK(problems("lvel = 3\n") == "f:2: unknown field 'lvel' (did you mean 'level'?)\n");
    CHECK(problems("zzz = 3\n") == "f:2: unknown field 'zzz'\n");
    CHECK(problems("[stats]\nhpp = 3\nhp = 1\n") ==
          "f:3: unknown field 'hpp' in [stats] (did you mean 'hp'?)\n");
    CHECK(problems("[[drops]]\nitem = \"a\"\nchanse = 1\n") ==
          "f:4: unknown field 'chanse' in [[drops]] (did you mean 'chance'?)\n");
}

NNH_TEST(data_convert_reports_missing_required_fields) {
    CHECK(convert("level = 3\n").problems == "f:1: missing required field 'name'\n");
    CHECK(problems("[stats]\nattack = 1\n") == "f:2: [stats]: missing required field 'hp'\n");
    CHECK(problems("[[drops]]\nchance = 0.5\n") == "f:2: [[drops]]: missing required field 'item'\n");
}

NNH_TEST(data_convert_reports_values_of_the_wrong_kind) {
    CHECK(problems("level = \"7\"\n") == "f:2: level: expected an integer, found a string\n");
    CHECK(problems("level = 7.0\n") == "f:2: level: expected an integer, found a float\n");
    CHECK(problems("speed = true\n") == "f:2: speed: expected a number, found a boolean\n");
    CHECK(problems("flags = [1, false, true]\n") ==
          "f:2: flags[0]: expected true or false, found an integer\n");
    CHECK(problems("flags = true\n") == "f:2: flags: expected an array, found a boolean\n");
    CHECK(problems("name2 = 1\n").find("unknown field") != std::string::npos);
    CHECK(problems("sprite = 3\n") == "f:2: sprite: expected a path in quotes, found an integer\n");
    CHECK(problems("element = 1\n") == "f:2: element: expected a name in quotes, found an integer\n");
}

NNH_TEST(data_convert_checks_integers_against_their_type_and_range) {
    CHECK(problems("level = 0\n") == "f:2: level: 0 is out of range: from 1 to 99\n");
    CHECK(problems("level = 300\n") == "f:2: level: 300 doesn't fit a u8 (0 to 255)\n");
    CHECK(problems("level = -1\n") == "f:2: level: -1 doesn't fit a u8 (0 to 255)\n");
    CHECK(problems("level = 99999999999999999999\n") ==
          "f:2: level: 99999999999999999999 doesn't fit a u8 (0 to 255)\n");
    CHECK(problems("[stats]\nhp = 1000\n") == "f:3: stats.hp: 1000 is out of range: from 1 to 999\n");
    CHECK(problems("[stats]\nhp = 1\nattack = -129\n") ==
          "f:4: stats.attack: -129 doesn't fit an i8 (-128 to 127)\n");
    CHECK(problems("price = -1\n", "item") == "f:1: price: -1 is out of range: at least 0\n");
    CHECK(problems("price = 2_147_483_647\n", "item") == "ok");
}

NNH_TEST(data_convert_checks_floats) {
    CHECK(problems("speed = 3\n") == "ok"); // an integer is fine when a float holds it exactly
    CHECK(problems("speed = 16777217\n") == "f:2: speed: 16777217 can't be stored exactly in a float\n");
    CHECK(problems("speed = -0.5\n") == "f:2: speed: -0.5 is out of range: at least 0\n");
    CHECK(problems("speed = 1e39\n") == "f:2: speed: 1e39 is outside a float's range\n");
    CHECK(problems("[[drops]]\nitem = \"a\"\nchance = 1.5\n") ==
          "f:4: drops[0].chance: 1.5 is out of range: from 0 to 1\n");
}

NNH_TEST(data_convert_checks_strings) {
    CHECK(problems("name = \"abcdefghijklmnopqrstuvwxyz\"\n", "item") == "f:1: name: 26 bytes, at most 16\n");
    CHECK(problems("name = \"\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\"\n", "item") ==
          "ok");
    CHECK(problems("name = \"\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7\xC3\xA7x\"\n",
                   "item") == "f:1: name: 17 bytes, at most 16\n");
}

NNH_TEST(data_convert_checks_enum_names_with_a_suggestion) {
    CHECK(problems("element = \"Fier\"\n") ==
          "f:2: element: unknown Element 'Fier' (did you mean 'Fire'?)\n");
    CHECK(problems("element = \"Lava\"\n") == "f:2: element: unknown Element 'Lava'\n");
    CHECK(problems("element = \"Ice\"\n") == "ok");
}

NNH_TEST(data_convert_checks_vectors_and_colours) {
    CHECK(problems("spawn = [1]\n") == "f:2: spawn: expected [x, y]\n");
    CHECK(problems("spawn = [1, \"a\"]\n") == "f:2: spawn: expected [x, y]\n");
    CHECK(problems("spawn = 1\n") == "f:2: spawn: expected [x, y]\n");
    CHECK(problems("tint = \"#FF0000\"\n") == "ok");
    CHECK(problems("tint = \"FF0000\"\n") == "f:2: tint: expected \"#RRGGBB\" or \"#RRGGBBAA\"\n");
    CHECK(problems("tint = \"xFF0000\"\n") == "f:2: tint: expected \"#RRGGBB\" or \"#RRGGBBAA\"\n");
    CHECK(problems("tint = \"#FF00GG\"\n") == "f:2: tint: expected \"#RRGGBB\" or \"#RRGGBBAA\"\n");
    CHECK(problems("tint = [255, 0, 0]\n") == "f:2: tint: expected \"#RRGGBB\" or \"#RRGGBBAA\"\n");
    const auto r = convert("name = \"x\"\ntint = \"#ff8000\"\n");
    Enemy e;
    CHECK(load_compiled(r.data.bytes, e) && e.tint.r == 255 && e.tint.g == 128 && e.tint.b == 0 &&
          e.tint.a == 255);
}

NNH_TEST(data_convert_checks_reference_paths) {
    CHECK(problems("sprite = \"../art/x\"\n") == "f:2: sprite: '../art/x' isn't an asset path\n");
    CHECK(problems("sprite = \"akhnp9x\"\n") ==
          "f:2: sprite: the path 'akhnp9x' has the hash 0, which means none: rename the asset\n");
    CHECK(problems("hurt = \"\"\n") == "ok");
}

NNH_TEST(data_convert_checks_array_and_list_lengths) {
    CHECK(problems("flags = [true, false]\n") == "f:2: flags: 2 values, exactly 3\n");
    CHECK(problems("flags = [true, false, true, true]\n") == "f:2: flags: 4 values, exactly 3\n");
    std::string five;
    for (int i = 0; i < 5; ++i)
        five += "[[drops]]\nitem = \"i\"\n";
    CHECK(problems(five) == "f:2: drops: 5 blocks, at most 4\n");
}

NNH_TEST(data_convert_reports_every_problem_sorted_by_line) {
    CHECK(problems("level = 0\nzzz = 1\nspeed = -1\n") ==
          "f:2: level: 0 is out of range: from 1 to 99\nf:3: unknown field 'zzz'\n"
          "f:4: speed: -1 is out of range: at least 0\n");
}

NNH_TEST(data_convert_stops_at_a_toml_problem) {
    CHECK(problems("level = 0\nspeed = 'fast'\nzzz = 1\n") ==
          "f:3: literal strings ('...') aren't supported: use \"...\"\n");
}
