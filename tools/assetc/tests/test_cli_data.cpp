// `nnh-assetc data` (spec 014 T5.4, FR-4, FR-9): a file whose extension is a declared key is converted and
// gets a references sidecar; any other file is copied with an empty sidecar; problems print `<shown
// path>:<line>: reason`, exit with 1 and leave no output.

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "cli_fixture.hpp"
#include "convert_fixture.hpp"
#include "data/schema_write.hpp"
#include "test.hpp"
#include "test_types.hpp"
#include "tiled_fixture.hpp"

using cli_test::exists;
using cli_test::run;

namespace {

const std::string kDir = NNH_ASSETC_TEST_TMP "/cli_data";

void append(void* context, const char* text, std::size_t size) {
    static_cast<std::string*>(context)->append(text, size);
}

/// The tests' schema description in cli_data/schema.nnhschema.
std::string schema_file() {
    std::string text;
    nnh::detail::write_schema(nnh::detail::data_registry(), append, &text);
    const std::string path = kDir + "/schema.nnhschema";
    tiled_fixture::write_file(path.c_str(), text);
    return path;
}

std::vector<std::string> args(const std::string& name, const std::string& in_text) {
    const std::string in = kDir + "/assets/" + name;
    tiled_fixture::write_file(in.c_str(), in_text);
    std::remove((kDir + "/out/" + name).c_str());
    std::remove((kDir + "/refs/" + name + ".refs").c_str());
    return {"data",
            in,
            kDir + "/out/" + name,
            "--schema",
            schema_file(),
            "--refs",
            kDir + "/refs/" + name + ".refs",
            "--name",
            name,
            "--show",
            "assets/" + name};
}

} // namespace

NNH_TEST(cli_data_converts_a_file_of_a_declared_type) {
    const auto r = run(args("goblin.enemy", "name = \"Goblin\"\nsprite = \"art/goblin\"\n"));
    CHECK(r.code == 0 && r.err.empty());
    const std::string bytes = cli_test::read(kDir + "/out/goblin.enemy");
    test_data::Enemy e;
    CHECK(convert_test::load_compiled(std::vector<std::uint8_t>(bytes.begin(), bytes.end()), e));
    CHECK(e.name == "Goblin");
    CHECK(cli_test::read(kDir + "/refs/goblin.enemy.refs") ==
          "nnh-data-refs 1\nasset enemy " + std::to_string(bytes.size()) +
              " goblin\nref 2 texture - sprite art/goblin\n");
}

NNH_TEST(cli_data_copies_other_files_with_an_empty_sidecar) {
    const auto r = run(args("notes.txt", "name = not data\x01"));
    CHECK(r.code == 0 && r.err.empty());
    CHECK(cli_test::read(kDir + "/out/notes.txt") == "name = not data\x01");
    CHECK(cli_test::read(kDir + "/refs/notes.txt.refs") == "nnh-data-refs 1\n");
}

NNH_TEST(cli_data_prints_every_problem_and_leaves_no_output) {
    std::vector<std::string> a = args("bat.enemy", "name = \"Bat\"\n");
    CHECK(run(a).code == 0); // an earlier good build left both outputs
    tiled_fixture::write_file(a[1].c_str(), "name = \"Bat\"\nlvel = 3\nspeed = -1\n");
    const auto r = run(a);
    CHECK(r.code == 1);
    CHECK(r.err == "assets/bat.enemy:2: unknown field 'lvel' (did you mean 'level'?)\n"
                   "assets/bat.enemy:3: speed: -1 is out of range: at least 0\n");
    CHECK(!exists(kDir + "/out/bat.enemy") && !exists(kDir + "/refs/bat.enemy.refs"));
}

NNH_TEST(cli_data_shows_the_input_path_when_no_show_path_is_given) {
    std::vector<std::string> a = args("rat.enemy", "speed = 1\n");
    a.resize(a.size() - 2);
    const auto r = run(a);
    CHECK(r.code == 1 && r.err == kDir + "/assets/rat.enemy:1: missing required field 'name'\n");
}

NNH_TEST(cli_data_reports_a_bad_schema_or_input) {
    std::vector<std::string> a = args("slime.enemy", "name = \"Slime\"\n");
    a[4] = kDir + "/missing.nnhschema";
    CHECK(run(a).err == kDir + "/missing.nnhschema: can't read the file\n");
    tiled_fixture::write_file((kDir + "/bad.nnhschema").c_str(), "nnh-data-schema 9\n");
    a[4] = kDir + "/bad.nnhschema";
    const auto bad = run(a);
    CHECK(bad.code == 1 && bad.err == kDir + "/bad.nnhschema: line 1: expected 'nnh-data-schema 1'\n");
    std::vector<std::string> b = args("ghost.enemy", "");
    b[1] = kDir + "/assets/none.enemy";
    const auto none = run(b);
    CHECK(none.code == 1 && none.err == kDir + "/assets/none.enemy: can't read the file\n");
}

NNH_TEST(cli_data_usage_errors_exit_with_2) {
    const std::vector<std::string> good = args("x.enemy", "name = \"x\"\n");
    for (std::size_t drop : {4u, 6u, 8u}) { // --schema, --refs or --name missing
        std::vector<std::string> a = good;
        a.erase(a.begin() + static_cast<long>(drop - 1), a.begin() + static_cast<long>(drop + 1));
        CHECK(run(a).code == 2);
    }
    CHECK(run({"data", "a", "b"}).code == 2);
    CHECK(run({"data", "a", "b", "--schema"}).code == 2);
}

NNH_TEST(cli_data_takes_the_extension_from_the_file_name_only) {
    std::filesystem::create_directories(kDir + "/assets/maps.enemy");
    std::filesystem::create_directories(kDir + "/out/maps.enemy");
    std::filesystem::create_directories(kDir + "/refs/maps.enemy");
    const auto r = run(args("maps.enemy/notes", "not = data at all"));
    CHECK(r.code == 0); // copied: "maps.enemy" is a folder, the file has no extension
    CHECK(cli_test::read(kDir + "/refs/maps.enemy/notes.refs") == "nnh-data-refs 1\n");
}
