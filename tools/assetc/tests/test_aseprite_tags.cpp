// Frame tags of Aseprite exports (spec 016 T7.3): clips from tags in every direction, `repeat` as a string or a
// number, `@` marker tags, the `default` clip, and refusals with `file:line: reason`.

#include <string>

#include "animation/sprite_format.hpp"
#include "aseprite_fixture.hpp"
#include "test.hpp"

using aseprite_fixture::Export;
using nnh::assetc::Problem;
using nnh::assetc::SpriteModel;
using namespace nnh::detail::spritefmt;

namespace {

/// Reads `e` with `tags`; the model's clips and markers.
SpriteModel tagged(int frames, const std::string& tags, bool* ok = nullptr, std::string* error = nullptr) {
    Export e = aseprite_fixture::row(frames);
    e.tags = tags;
    SpriteModel m;
    Problem p;
    const bool read = aseprite_fixture::read(e, m, p);
    if (ok != nullptr) *ok = read;
    if (error != nullptr) *error = nnh::assetc::problem_text(p);
    return m;
}

std::string refusal(int frames, const std::string& tags) {
    bool ok = true;
    std::string error;
    tagged(frames, tags, &ok, &error);
    return ok ? "accepted" : error;
}

bool says(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

} // namespace

NNH_TEST(aseprite_tags_become_clips_in_every_direction) {
    const SpriteModel m = tagged(8, R"({"name": "idle", "from": 0, "to": 1, "direction": "forward", "color": "#000000ff"},
        {"name": "back", "from": 2, "to": 4, "direction": "reverse"},
        {"name": "pp", "from": 4, "to": 7, "direction": "pingpong"},
        {"name": "ppr", "from": 4, "to": 7, "direction": "pingpong_reverse", "data": "user data"})");
    CHECK(m.clips.size() == 4);
    CHECK(m.clips[0].name == "idle" && m.clips[0].from == 0 && m.clips[0].to == 1 && m.clips[0].mode == kLoop);
    CHECK(m.clips[1].direction == kReverse && m.clips[1].mode == kLoop);
    CHECK(m.clips[2].mode == kPingPong && m.clips[2].count == 0 && m.clips[2].direction == kForward);
    CHECK(m.clips[3].mode == kPingPong && m.clips[3].direction == kReverse);
    CHECK(m.clips[1].line == 15); // frames on lines 3..10, then 3 lines, the tags from line 14 (one per line)
}

NNH_TEST(aseprite_repeat_is_read_as_a_string_or_a_number) {
    const SpriteModel m = tagged(4, R"({"name": "once", "from": 0, "to": 1, "direction": "forward", "repeat": "1"},
        {"name": "three", "from": 0, "to": 1, "direction": "reverse", "repeat": "3"},
        {"name": "num", "from": 0, "to": 1, "direction": "forward", "repeat": 2},
        {"name": "pp", "from": 0, "to": 3, "direction": "pingpong", "repeat": "3"},
        {"name": "zero", "from": 0, "to": 1, "direction": "forward", "repeat": "0"})");
    CHECK(m.clips[0].mode == kOnce && m.clips[0].count == 1);
    CHECK(m.clips[1].mode == kRepeat && m.clips[1].count == 3 && m.clips[1].direction == kReverse);
    CHECK(m.clips[2].mode == kRepeat && m.clips[2].count == 2);
    CHECK(m.clips[3].mode == kPingPong && m.clips[3].count == 3); // passes, as Aseprite counts them
    CHECK(m.clips[4].mode == kLoop);
}

NNH_TEST(aseprite_marker_tags_put_markers_on_frames) {
    const SpriteModel m = tagged(4, R"({"name": "run", "from": 0, "to": 3, "direction": "forward"},
        {"name": "@step", "from": 1, "to": 1, "direction": "forward"},
        {"name": "@hit=-2", "from": 2, "to": 3, "direction": "forward"})");
    CHECK(m.clips.size() == 1 && m.markers.size() == 2);
    CHECK(m.markers[0].name == "step" && m.markers[0].value == 0 && m.markers[0].from == 1 && m.markers[0].to == 1);
    CHECK(m.markers[1].name == "hit" && m.markers[1].value == -2 && m.markers[1].to == 3);
}

NNH_TEST(aseprite_without_clip_tags_has_one_default_clip) {
    const SpriteModel m = tagged(5, R"({"name": "@step", "from": 1, "to": 1, "direction": "forward"})");
    CHECK(m.clips.size() == 1 && m.clips[0].name == "default" && m.clips[0].from == 0 && m.clips[0].to == 4);
    CHECK(m.clips[0].mode == kLoop && m.markers.size() == 1);
    CHECK(tagged(2, "").clips.size() == 1);
}

NNH_TEST(aseprite_tag_refusals_name_the_line) {
    CHECK(says(refusal(4, R"({"name": "a", "from": 0, "to": 1, "direction": "sideways"})"),
               "hero.ase.json:10: frame tag \"a\": unknown direction \"sideways\""));
    CHECK(says(refusal(4, R"({"name": "a", "from": 0, "to": 4, "direction": "forward"})"),
               ":10: frame tag \"a\": \"to\" must be a whole number from 0 to 3 (the export has 4 frames)"));
    CHECK(says(refusal(4, R"({"name": "a", "from": 2, "to": 1})"), ":10: frame tag \"a\": \"from\" is after \"to\""));
    CHECK(says(refusal(4, R"({"name": "a", "from": 0, "to": 1}, {"name": "a", "from": 2, "to": 3})"),
               ":10: frame tag \"a\": two clips have this name"));
    CHECK(says(refusal(4, R"({"name": "a", "from": 0, "to": 1, "repeat": "x2"})"),
               "\"repeat\" must be a whole number from 0 to 65535"));
    CHECK(says(refusal(4, R"({"name": "a", "from": 0, "to": 1, "repeat": "70000"})"), "\"repeat\" must be"));
    CHECK(says(refusal(4, R"({"name": "@hit=lots", "from": 0, "to": 1})"),
               "marker tag \"@hit=lots\": the value after '=' must be a whole number"));
    CHECK(says(refusal(4, R"({"name": "@", "from": 0, "to": 1})"), "the name after '@' must be 1 to 31 bytes"));
    CHECK(says(refusal(4, R"({"name": "@a=", "from": 0, "to": 1})"), "the value after '='"));
    CHECK(says(refusal(4, R"({"name": "0123456789012345678901234567890123", "from": 0, "to": 1})"),
               "the name must be 1 to 31 bytes"));
    CHECK(says(refusal(4, R"({"from": 0, "to": 1})"), "frame tag: \"name\" is missing"));
    CHECK(says(refusal(4, R"(7)"), "a frame tag is not an object"));
}

NNH_TEST(aseprite_over_256_frames_need_tags) {
    CHECK(says(refusal(257, ""), "more than 256 frames and no frame tags"));
    CHECK(says(refusal(300, R"({"name": "a", "from": 0, "to": 256})"), "frame tag \"a\": more than 256 frames"));
}
