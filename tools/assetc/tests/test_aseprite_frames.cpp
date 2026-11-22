// Frames and meta of Aseprite exports (spec 016 T7.2): Hash and Array layouts, trim, durations, meta.image and
// meta.size against the PNG, and refusals with `file:line: reason`.

#include <string>

#include "aseprite_fixture.hpp"
#include "test.hpp"

using aseprite_fixture::Export;
using nnh::assetc::Problem;
using nnh::assetc::SpriteModel;

namespace {

/// The problem of converting `e` as `file:line: reason`.
std::string refusal(const Export& e) {
    nnh::assetc::Converted out;
    Problem p;
    if (aseprite_fixture::convert(e, out, p)) return "converted";
    return nnh::assetc::problem_text(p);
}

bool says(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

} // namespace

NNH_TEST(aseprite_hash_frames_are_read_in_file_order) {
    Export e = aseprite_fixture::row(4);
    e.frames[2].ms = 70;
    SpriteModel m;
    Problem p;
    CHECK(aseprite_fixture::read(e, m, p));
    CHECK(m.frames.size() == 4);
    CHECK(m.frames[1].x == 16 && m.frames[3].x == 48 && m.frames[0].w == 16 && m.frames[0].h == 16);
    CHECK(m.frames[0].duration == 24576); // 100 ms = 6 frames exactly
    CHECK(m.frames[2].duration == 17203); // 70 ms = 17 203.2 ticks, rounded
    CHECK(m.frames[3].line == 6);
}

NNH_TEST(aseprite_array_frames_read_the_same) {
    Export hash = aseprite_fixture::row(3);
    Export array = hash;
    array.array = true;
    SpriteModel a, b;
    Problem p;
    CHECK(aseprite_fixture::read(hash, a, p) && aseprite_fixture::read(array, b, p));
    CHECK(a.frames.size() == b.frames.size() && b.frames[2].x == 32 && b.frames[2].duration == a.frames[2].duration);
}

NNH_TEST(aseprite_trimmed_frames_keep_their_offset_and_source_size) {
    Export e = aseprite_fixture::row(2);
    e.frames[1] = {.x = 16, .y = 0, .w = 10, .h = 12, .sx = 3, .sy = 4, .source_w = 16, .source_h = 16};
    SpriteModel m;
    Problem p;
    CHECK(aseprite_fixture::read(e, m, p));
    CHECK(m.frames[1].offset_x == 3 && m.frames[1].offset_y == 4 && m.frames[1].source_w == 16);
}

NNH_TEST(aseprite_frame_refusals_name_the_line) {
    Export rotated = aseprite_fixture::row(3);
    rotated.frames[1].extra = "\"rotated\": true, ";
    CHECK(says(refusal(rotated), "hero.ase.json:4: frame 1 (\"hero 1.aseprite\"): rotated frames aren't supported"));
    Export zero = aseprite_fixture::row(3);
    zero.frames[2].ms = 0;
    CHECK(says(refusal(zero), "hero.ase.json:5: frame 2 (\"hero 2.aseprite\"): \"duration\" must be a whole number "
                              "from 1 to 65535"));
    Export long_frame = aseprite_fixture::row(1);
    long_frame.frames[0].ms = 65536;
    CHECK(says(refusal(long_frame), ":3: frame 0"));
    Export trimmed = aseprite_fixture::row(2);
    trimmed.frames[0].sx = 4; // a 16-pixel frame placed at 4 in a 16-pixel source
    CHECK(says(refusal(trimmed), ":3: frame 0 (\"hero 0.aseprite\"): the trimmed frame doesn't fit"));
}

NNH_TEST(aseprite_meta_must_match_the_png_beside_it) {
    Export image = aseprite_fixture::row(2);
    image.image = "other.png";
    CHECK(says(refusal(image), "\"image\" is \"other.png\": it must be hero.png"));
    Export size = aseprite_fixture::row(2);
    size.sheet_w = 64;
    CHECK(says(refusal(size), "\"size\" is 64x16 but hero.png is 32x16"));
    Export outside = aseprite_fixture::row(2);
    outside.frames[1].x = 20;
    CHECK(says(refusal(outside), "hero.ase.json:4: frame 1 is outside the hero.png image"));
    Export missing = aseprite_fixture::row(2);
    missing.png = false;
    CHECK(says(refusal(missing), "hero.ase.json: hero.png not found"));
    Export empty = aseprite_fixture::row(0);
    CHECK(says(refusal(empty), "the export has no frames"));
}

NNH_TEST(aseprite_files_that_are_not_exports_are_refused) {
    nnh::assetc::Converted out;
    Problem p;
    CHECK(!aseprite_fixture::convert({}, out, p, "[1, 2]"));
    CHECK(says(nnh::assetc::problem_text(p), "hero.ase.json:1: not an Aseprite export"));
    CHECK(!aseprite_fixture::convert({}, out, p, "{\"frames\": {}, \n\"meta\": {}}"));
    CHECK(!aseprite_fixture::convert({}, out, p, "{\"frames\": [1,"));
    CHECK(says(nnh::assetc::problem_text(p), "line 1, column"));
}
