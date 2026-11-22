// Ninja depfiles written by nnh-assetc (spec 012 T5.5, constitution III).

#include "depfile.hpp"
#include "test.hpp"

using nnh::assetc::depfile_text;

NNH_TEST(depfile_lists_the_target_and_its_inputs) {
    CHECK(depfile_text("/b/out.nnhmap", {"/a/x.tsj", "/a/y.png"}) == "/b/out.nnhmap: /a/x.tsj /a/y.png\n");
    CHECK(depfile_text("C:/b/o.nnhmap", {"C:/a/x.tsj"}) == "C:/b/o.nnhmap: C:/a/x.tsj\n");
    CHECK(depfile_text("out", {}) == "out:\n");
}

NNH_TEST(depfile_escapes_spaces_hashes_and_dollars) {
    CHECK(depfile_text("/my game/o", {"/my game/a b.tsj"}) == "/my\\ game/o: /my\\ game/a\\ b.tsj\n");
    CHECK(depfile_text("o", {"/a#1/$x.png"}) == "o: /a\\#1/$$x.png\n");
}
