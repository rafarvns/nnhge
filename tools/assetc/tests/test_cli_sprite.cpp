// The `sprite` command of nnh-assetc (spec 016 T7.4): writes the `.nnhsprite` and a depfile listing the export and
// its PNG; failures print `file:line: reason` and leave no output; usage errors exit with 2.

#include <cstdio>
#include <filesystem>
#include <string>

#include "animation/sprite_validate.hpp"
#include "aseprite_fixture.hpp"
#include "cli_fixture.hpp"
#include "paths.hpp"
#include "test.hpp"
#include "tiled_fixture.hpp"

namespace {

const std::string kDir = NNH_ASSETC_TEST_TMP "/cli";
const std::string kAssets = kDir + "/assets";

std::vector<std::string> sprite_args() {
    return {"sprite", kAssets + "/art/hero.ase.json", kDir + "/out/hero.nnhsprite", "--assets", kAssets, "--depfile",
            kDir + "/out/hero.nnhsprite.d"};
}

void make_files(const std::string& json) {
    tiled_fixture::write_file((kAssets + "/art/hero.png").c_str(), tiled_fixture::png(64, 16));
    tiled_fixture::write_file((kAssets + "/art/hero.ase.json").c_str(), json);
}

} // namespace

NNH_TEST(cli_sprite_writes_a_valid_library_and_its_depfile) {
    aseprite_fixture::Export e = aseprite_fixture::row(4);
    e.tags = R"({"name": "run", "from": 0, "to": 3, "direction": "pingpong"})";
    make_files(aseprite_fixture::json(e));
    std::remove((kDir + "/out/hero.nnhsprite").c_str());
    const cli_test::Run r = cli_test::run(sprite_args());
    CHECK(r.code == 0 && r.err.empty());
    const std::string bytes = cli_test::read(kDir + "/out/hero.nnhsprite");
    CHECK(nnh::detail::validate_sprite_library(bytes.data(), bytes.size()).ok());
    const std::string dep = cli_test::read(kDir + "/out/hero.nnhsprite.d");
    const std::string cwd = std::filesystem::current_path().generic_string();
    auto rel = [&](const std::string& p) { return nnh::assetc::relative_path(cwd, p); };
    CHECK(dep == rel(kDir + "/out/hero.nnhsprite") + ": " + rel(kAssets + "/art/hero.ase.json") + " " +
                     rel(kAssets + "/art/hero.png") + "\n");
}

NNH_TEST(cli_sprite_failures_name_the_line_and_leave_no_output) {
    aseprite_fixture::Export e = aseprite_fixture::row(4);
    e.tags = R"({"name": "run", "from": 0, "to": 3, "direction": "upward"})";
    make_files(aseprite_fixture::json(e));
    tiled_fixture::write_file((kDir + "/out/hero.nnhsprite").c_str(), "stale");
    const cli_test::Run r = cli_test::run(sprite_args());
    CHECK(r.code == 1);
    CHECK(r.err.find("hero.ase.json:10: frame tag \"run\": unknown direction \"upward\"") != std::string::npos);
    CHECK(!cli_test::exists(kDir + "/out/hero.nnhsprite"));
}

NNH_TEST(cli_sprite_usage_errors_exit_with_2) {
    CHECK(cli_test::run({"sprite", "a.ase.json"}).code == 2);
    CHECK(cli_test::run({"sprite", "a.ase.json", "a.nnhsprite"}).code == 2); // --assets is required
}
