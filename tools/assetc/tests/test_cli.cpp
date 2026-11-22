// The nnh-assetc command line (spec 012 T5.5): `map` writes the `.nnhmap` and its depfile, failures print
// `file: reason` and leave no output, usage errors exit with 2.

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "cli.hpp"
#include "files.hpp"
#include "paths.hpp"
#include "test.hpp"
#include "tiled_fixture.hpp"
#include "tilemap/map_validate.hpp"

namespace {

const std::string kDir = NNH_ASSETC_TEST_TMP "/cli";
const std::string kAssets = kDir + "/assets";

struct Run {
    int code = 0;
    std::string out, err;
};

std::string read_back(std::FILE* f) {
    std::string text;
    std::rewind(f);
    char buffer[512];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0) text.append(buffer, got);
    std::fclose(f);
    return text;
}

Run run(std::vector<std::string> args) {
    std::vector<const char*> argv{"nnh-assetc"};
    for (const std::string& a : args) argv.push_back(a.c_str());
    std::FILE* out = std::tmpfile();
    std::FILE* err = std::tmpfile();
    Run r;
    r.code = nnh::assetc::run_cli(static_cast<int>(argv.size()), argv.data(), out, err);
    r.out = read_back(out);
    r.err = read_back(err);
    return r;
}

bool exists(const std::string& path) {
    std::string bytes;
    return nnh::assetc::DiskFiles{}.read(path, 1, bytes);
}

/// assets/levels/a.tmj (uses ../tiles/w.tsj), assets/tiles/w.tsj, assets/tiles/w.png; `map` replaces a.tmj.
void make_files(const std::string& map = "") {
    std::remove((kAssets + "/levels/a.tmj").c_str());
    // The folders exist once the first test made them: created by the test's CMake step (see CMakeLists.txt).
    tiled_fixture::write_file((kAssets + "/tiles/w.png").c_str(), tiled_fixture::png(64, 32));
    tiled_fixture::write_file((kAssets + "/tiles/w.tsj").c_str(),
                              R"({"image": "w.png", "imagewidth": 64, "imageheight": 32, "tilewidth": 16,
                                  "tileheight": 16, "tilecount": 8, "columns": 4})");
    tiled_fixture::write_file((kAssets + "/levels/a.tmj").c_str(),
                              map.empty() ? tiled_fixture::map_json(R"(, "tilesets": [{"firstgid": 1,
                                  "source": "../tiles/w.tsj"}], "layers": [{"type": "tilelayer", "name": "g",
                                  "width": 4, "height": 2, "data": [1, 2, 3, 4, 0, 0, 0, 8]}])")
                                          : map);
}

std::vector<std::string> map_args() {
    return {"map", kAssets + "/levels/a.tmj", kDir + "/out/a.nnhmap", "--assets", kAssets, "--depfile",
            kDir + "/out/a.nnhmap.d"};
}

} // namespace

NNH_TEST(cli_version_names_the_tool_and_the_shared_crc) {
    const Run r = run({"--version"});
    CHECK(r.code == 0);
    CHECK(r.out == "nnh-assetc " NNH_ASSETC_VERSION " (crc32 CBF43926)\n");
}

NNH_TEST(cli_usage_errors_exit_with_2) {
    CHECK(run({}).code == 2);
    CHECK(run({"bogus"}).code == 2);
    CHECK(run({"map", "a.tmj"}).code == 2);
    CHECK(run({"map", "a.tmj", "b.nnhmap"}).code == 2); // --assets is required
    CHECK(run({"map", "a.tmj", "b.nnhmap", "--assets"}).code == 2);
    const Run r = run({"map", "a.tmj", "b.nnhmap", "--assets", "x", "--what", "y"});
    CHECK(r.code == 2 && r.err.find("usage:") != std::string::npos);
}

NNH_TEST(cli_map_writes_a_valid_map_and_its_depfile) {
    make_files();
    std::remove((kDir + "/out/a.nnhmap").c_str());
    std::remove((kDir + "/out/a.nnhmap.d").c_str());
    const Run r = run(map_args());
    CHECK(r.code == 0 && r.err.empty());
    std::string bytes;
    CHECK(nnh::assetc::DiskFiles{}.read(kDir + "/out/a.nnhmap", 1 << 20, bytes));
    CHECK(nnh::detail::validate_map(bytes.data(), bytes.size()).ok());
    std::string dep;
    CHECK(nnh::assetc::DiskFiles{}.read(kDir + "/out/a.nnhmap.d", 1 << 16, dep));
    // Relative to the working directory: CMake reads depfile paths from there, and devkitPro's MSYS CMake would
    // misread the native tool's absolute "E:/..." paths.
    const std::string cwd = std::filesystem::current_path().generic_string();
    auto rel = [&](const std::string& p) { return nnh::assetc::relative_path(cwd, p); };
    CHECK(dep == rel(kDir + "/out/a.nnhmap") + ": " + rel(kAssets + "/levels/a.tmj") + " " +
                     rel(kAssets + "/tiles/w.tsj") + " " + rel(kAssets + "/tiles/w.png") + "\n");
    CHECK(dep.find(':') == dep.find(": ")); // no drive letter left
}

NNH_TEST(cli_map_failure_prints_file_and_reason_and_leaves_no_output) {
    make_files();
    CHECK(run(map_args()).code == 0);
    make_files(R"({"orientation": "isometric", "width": 4, "height": 2, "tilewidth": 16, "tileheight": 16})");
    const Run r = run(map_args());
    CHECK(r.code == 1);
    CHECK(r.err == kAssets + "/levels/a.tmj: isometric maps aren't supported: NNHGE draws orthogonal maps "
                             "(Map > Map Properties > Orientation)\n");
    CHECK(!exists(kDir + "/out/a.nnhmap"));
}

NNH_TEST(cli_map_names_the_tileset_file_when_the_problem_is_there) {
    make_files();
    tiled_fixture::write_file((kAssets + "/tiles/w.tsj").c_str(), R"({"image": "w.png", "wangsets": [{}]})");
    const Run r = run(map_args());
    CHECK(r.code == 1);
    CHECK(r.err.rfind(kAssets + "/tiles/w.tsj: wang sets", 0) == 0);
}

NNH_TEST(cli_map_refuses_a_map_over_the_limits) {
    std::string objects;
    for (int i = 1; i <= 4097; ++i) objects += (i > 1 ? "," : "") + std::string(R"({"id":)") + std::to_string(i) + R"(,"x":0,"y":0})";
    make_files(tiled_fixture::map_json(R"(, "layers": [{"type": "objectgroup", "objects": [)" + objects + "]}]"));
    const Run r = run(map_args());
    CHECK(r.code == 1 && r.err.find("a.tmj: the map has more than 4096 objects") != std::string::npos);
}

NNH_TEST(cli_map_says_when_it_cannot_write) {
    make_files();
    std::vector<std::string> args = map_args();
    args[2] = kDir + "/no_such_folder/a.nnhmap";
    const Run r = run(args);
    CHECK(r.code == 1 && r.err.find("no_such_folder/a.nnhmap: can't write the file") != std::string::npos);
}
