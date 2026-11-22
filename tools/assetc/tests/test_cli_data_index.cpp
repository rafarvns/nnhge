// `nnh-assetc data-index` (spec 014 T6.2, FR-9, FR-12): the sidecars of a game's data assets and its asset list
// become `.nnh_data`; problems print `<file>:<line>: reason`, exit with 1 and leave no index; the index (and the
// data step's outputs) are rewritten only when their bytes change, so the build's later steps don't rerun.

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "cli_fixture.hpp"
#include "data/index_validate.hpp"
#include "data/schema_write.hpp"
#include "test.hpp"
#include "tiled_fixture.hpp"

using cli_test::exists;
using cli_test::run;

namespace {

const std::string kDir = NNH_ASSETC_TEST_TMP "/cli_index";

void append(void* context, const char* text, std::size_t size) {
    static_cast<std::string*>(context)->append(text, size);
}

void write(const std::string& path, const std::string& text) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    tiled_fixture::write_file(path.c_str(), text);
}

std::string schema_file() {
    std::string text;
    nnh::detail::write_schema(nnh::detail::data_registry(), append, &text);
    write(kDir + "/schema.nnhschema", text);
    return kDir + "/schema.nnhschema";
}

/// A game with a goblin (texture art/goblin) and a potion; `goblin_refs` replaces the goblin's sidecar.
std::vector<std::string> args(const std::string& goblin_refs = "nnh-data-refs 1\nasset enemy 50 enemies/goblin\n"
                                                                "ref 3 texture - sprite art/goblin\n") {
    write(kDir + "/refs/enemies/goblin.enemy.refs", goblin_refs);
    write(kDir + "/refs/items/potion.item.refs", "nnh-data-refs 1\nasset item 30 items/potion\n");
    write(kDir + "/refs/notes.txt.refs", "nnh-data-refs 1\n");
    write(kDir + "/refs.list", kDir + "/refs/enemies/goblin.enemy.refs\n" + kDir + "/refs/items/potion.item.refs\n" +
                                   kDir + "/refs/notes.txt.refs\n");
    write(kDir + "/assets.list", "art/goblin.png\nenemies/goblin.enemy\nitems/potion.item\nnotes.txt\n");
    return {"data-index", "--out",        kDir + "/out/.nnh_data", "--schema", schema_file(),
            "--assets-list", kDir + "/assets.list", "--refs-list", kDir + "/refs.list", "--show", "game/assets"};
}

std::filesystem::file_time_type old_time() {
    return std::filesystem::file_time_type::clock::now() - std::chrono::hours(48);
}

} // namespace

NNH_TEST(cli_data_index_writes_an_index_the_engine_accepts) {
    std::filesystem::create_directories(kDir + "/out");
    const auto r = run(args());
    CHECK(r.code == 0 && r.err.empty());
    const std::string bytes = cli_test::read(kDir + "/out/.nnh_data");
    CHECK(nnh::detail::validate_index(bytes.data(), bytes.size()).ok());
    const nnh::detail::IndexView view(bytes.data());
    CHECK(view.entry_count() == 3 && view.largest() == 50);
}

NNH_TEST(cli_data_index_reads_sidecar_paths_relative_to_the_list) {
    std::filesystem::create_directories(kDir + "/out");
    std::vector<std::string> a = args();
    // devkitPro's MSYS CMake writes paths this native program can't open as absolute ones: the build lists them
    // relative to the list's folder.
    write(kDir + "/refs.list", "refs/enemies/goblin.enemy.refs\nrefs/items/potion.item.refs\n");
    CHECK(run(a).code == 0);
    const std::string bytes = cli_test::read(kDir + "/out/.nnh_data");
    CHECK(nnh::detail::IndexView(bytes.data()).entry_count() == 3);
}

NNH_TEST(cli_data_index_reads_lists_with_crlf_and_blank_lines) {
    std::filesystem::create_directories(kDir + "/out");
    std::vector<std::string> a = args();
    // CMake writes these lists with CRLF on Windows.
    write(kDir + "/refs.list", "refs/enemies/goblin.enemy.refs\r\n\r\nrefs/items/potion.item.refs\r\n");
    write(kDir + "/assets.list", "art/goblin.png\r\n\r\nenemies/goblin.enemy\r\nitems/potion.item\r\n");
    const auto r = run(a);
    CHECK(r.code == 0 && r.err.empty());
    const std::string bytes = cli_test::read(kDir + "/out/.nnh_data");
    CHECK(nnh::detail::IndexView(bytes.data()).entry_count() == 3);
}

NNH_TEST(write_file_if_changed_replaces_a_longer_file_that_starts_the_same) {
    const std::string path = kDir + "/prefix.bin";
    write(path, "abcd");
    CHECK(nnh::assetc::write_file_if_changed(path, "abc", 3));
    CHECK(cli_test::read(path) == "abc");
    CHECK(nnh::assetc::write_file_if_changed(path, "abcde", 5));
    CHECK(cli_test::read(path) == "abcde");
}

NNH_TEST(cli_data_index_rewrites_the_index_only_when_it_changes) {
    std::filesystem::create_directories(kDir + "/out");
    CHECK(run(args()).code == 0);
    const std::string out = kDir + "/out/.nnh_data";
    std::filesystem::last_write_time(out, old_time());
    const auto before = std::filesystem::last_write_time(out);
    CHECK(run(args()).code == 0);
    CHECK(std::filesystem::last_write_time(out) == before);
    CHECK(run(args("nnh-data-refs 1\nasset enemy 51 enemies/goblin\n")).code == 0);
    CHECK(std::filesystem::last_write_time(out) != before);
}

NNH_TEST(cli_data_index_prints_the_problems_and_leaves_no_index) {
    std::filesystem::create_directories(kDir + "/out");
    CHECK(run(args()).code == 0);
    const auto r = run(args("nnh-data-refs 1\nasset enemy 50 enemies/goblin\nref 4 data item drops[0].item "
                            "items/potoin\n"));
    CHECK(r.code == 1);
    CHECK(r.err == "game/assets/enemies/goblin.enemy:4: drops[0].item: no item 'items/potoin' (no file "
                   "game/assets/items/potoin.item; did you mean 'items/potion'?)\n");
    CHECK(!exists(kDir + "/out/.nnh_data"));
}

NNH_TEST(cli_data_index_reports_a_bad_or_missing_sidecar) {
    const auto bad = run(args("nnh-data-refs 1\nasset enemy\n"));
    CHECK(bad.code == 1 && bad.err == kDir + "/refs/enemies/goblin.enemy.refs: line 2: expected 'asset <key> "
                                             "<size> <name>'\n");
    std::vector<std::string> a = args();
    write(kDir + "/refs.list", kDir + "/refs/none.refs\n");
    const auto none = run(a);
    CHECK(none.code == 1 && none.err == kDir + "/refs/none.refs: can't read the file\n");
}

NNH_TEST(cli_data_index_reports_a_missing_list_or_schema) {
    std::vector<std::string> a = args();
    a[6] = kDir + "/none.list";
    CHECK(run(a).err == kDir + "/none.list: can't read the file\n");
    a = args();
    a[4] = kDir + "/none.nnhschema";
    CHECK(run(a).err == kDir + "/none.nnhschema: can't read the file\n");
}

NNH_TEST(cli_data_index_usage_errors_exit_with_2) {
    const std::vector<std::string> good = args();
    for (std::size_t drop : {1u, 3u, 5u, 7u}) { // --out, --schema, --assets-list or --refs-list missing
        std::vector<std::string> a = good;
        a.erase(a.begin() + static_cast<long>(drop), a.begin() + static_cast<long>(drop + 2));
        CHECK(run(a).code == 2);
    }
    CHECK(run({"data-index", "--out"}).code == 2);
    CHECK(run({"data-index", "--what", "x"}).code == 2);
}

NNH_TEST(cli_data_rewrites_its_outputs_only_when_they_change) {
    for (const char* d : {"/assets", "/out", "/refs"})
        std::filesystem::create_directories(kDir + d);
    write(kDir + "/assets/bat.enemy", "name = \"Bat\" # first\n");
    const std::vector<std::string> a = {"data",   kDir + "/assets/bat.enemy", kDir + "/out/bat.enemy",
                                        "--schema", schema_file(),           "--refs",
                                        kDir + "/refs/bat.enemy.refs", "--name", "bat.enemy"};
    CHECK(run(a).code == 0);
    const auto old = old_time();
    for (const char* f : {"/out/bat.enemy", "/refs/bat.enemy.refs"})
        std::filesystem::last_write_time(kDir + f, old);
    const auto out_time = std::filesystem::last_write_time(kDir + "/out/bat.enemy");
    write(kDir + "/assets/bat.enemy", "name = \"Bat\" # a comment changed: same bytes out\n");
    CHECK(run(a).code == 0);
    CHECK(std::filesystem::last_write_time(kDir + "/out/bat.enemy") == out_time);
    CHECK(std::filesystem::last_write_time(kDir + "/refs/bat.enemy.refs") == out_time);
    write(kDir + "/assets/bat.enemy", "name = \"Bats\"\n");
    CHECK(run(a).code == 0);
    CHECK(std::filesystem::last_write_time(kDir + "/out/bat.enemy") != out_time);
}
