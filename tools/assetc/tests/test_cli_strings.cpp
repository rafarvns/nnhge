// `nnh-assetc strings` and `strings-index` end to end (spec 029 T5.5, T5.6): a language list and string files on disk,
// each language's table and the manifest written and accepted by the engine's validators, the pseudo-language only in
// Debug/Development form. Regression: two structs named nnh::assetc::IndexInput (the data index's and the strings
// index's) broke the One Definition Rule and crashed strings-index.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "cli_fixture.hpp"
#include "files.hpp"
#include "loc/loc_file.hpp"
#include "test.hpp"
#include "tiled_fixture.hpp"

namespace {

namespace lf = nnh::detail::loc;

const std::string kDir = NNH_ASSETC_TEST_TMP "/cli";

using cli_test::run;
using cli_test::Run;

std::vector<std::uint8_t> read_all(const std::string& path) {
    std::vector<std::uint8_t> out;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return out;
    int c;
    while ((c = std::fgetc(f)) != EOF) out.push_back(static_cast<std::uint8_t>(c));
    std::fclose(f);
    return out;
}

void write_text(const std::string& path, const std::string& text) { tiled_fixture::write_file(path.c_str(), text); }

/// en (base, two files) and ja; the list the build writes.
std::string make_files() {
    write_text(kDir + "/en_a.strings", "[language]\nname = \"English\"\n[menu]\nnew_game = \"New game\"\n"
                                       "[items]\ncount.one = \"{count} item\"\ncount.other = \"{count} items\"\n");
    write_text(kDir + "/en_b.strings", "[menu]\nquit = \"Quit\"\n");
    write_text(kDir + "/ja.strings", "[language]\nname = \"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\"\n[menu]\n"
                                     "new_game = \"\xE3\x81\xAF\xE3\x81\x98\xE3\x82\x81\"\n"
                                     "[items]\ncount.other = \"{count}\"\n");
    const std::string list = kDir + "/langs.list";
    write_text(list, "*en\t" + kDir + "/en_a.strings;" + kDir + "/en_b.strings\nja\t" + kDir + "/ja.strings\n");
    return list;
}

} // namespace

NNH_TEST(cli_strings_writes_each_table_and_the_manifest) {
    const std::string list = make_files();
    for (const char* code : {"en", "ja"}) {
        const std::string out = kDir + "/" + code + ".nnhstr";
        const Run r = run({"strings", code, "--languages", list, "--out", out});
        CHECK(r.code == 0);
        const std::vector<std::uint8_t> t = read_all(out);
        CHECK(lf::validate_table(t.data(), t.size()) == nullptr);
    }
    const Run r = run({"strings-index", "--languages", list, "--out", kDir + "/m.nnh_loc", "--pseudo-out",
                       kDir + "/qps.nnhstr"});
    CHECK(r.code == 0);
    const std::vector<std::uint8_t> m = read_all(kDir + "/m.nnh_loc");
    CHECK(lf::validate_manifest(m.data(), m.size()) == nullptr);
    lf::ManifestHeader h;
    if (m.size() >= sizeof h) {
        std::memcpy(&h, m.data(), sizeof h);
        CHECK(h.language_count == 3); // en, ja and qps
        CHECK(h.base == 0);
        CHECK(h.pseudo == 2);
    }
    const std::vector<std::uint8_t> q = read_all(kDir + "/qps.nnhstr");
    CHECK(lf::validate_table(q.data(), q.size()) == nullptr);
    const Run release = run({"strings-index", "--languages", list, "--out", kDir + "/r.nnh_loc", "--release"});
    CHECK(release.code == 0);
    const std::vector<std::uint8_t> rm = read_all(kDir + "/r.nnh_loc");
    if (rm.size() >= sizeof h) {
        std::memcpy(&h, rm.data(), sizeof h);
        CHECK(h.language_count == 2);
        CHECK(h.pseudo == lf::kNoIndex);
    }
}

NNH_TEST(cli_strings_refuses_a_bad_list_and_usage) {
    CHECK(run({"strings-index", "--languages"}).code == 2);
    write_text(kDir + "/bad.list", "no tab here\n");
    const Run r = run({"strings-index", "--languages", kDir + "/bad.list", "--out", kDir + "/x.nnh_loc"});
    CHECK(r.code == 1);
    CHECK(r.err.find("not a language list") != std::string::npos);
}
