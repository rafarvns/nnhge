// Writing converted Aseprite exports (spec 016 T7.4): a valid `.nnhsprite` with capacities equal to the counts, the
// clips' ends and durations, markers on every clip frame their tags cover, the pivot on every frame, the
// dependencies; and the format's limits refused with the line at fault.

#include <cstring>
#include <string>

#include "animation/sprite_format.hpp"
#include "animation/sprite_validate.hpp"
#include "aseprite_fixture.hpp"
#include "nnh/detail/name_hash.hpp"
#include "test.hpp"

using aseprite_fixture::Export;
using nnh::assetc::Converted;
using nnh::assetc::Problem;
using namespace nnh::detail::spritefmt;

namespace {

template <typename T>
T record(const Converted& c, int section, int i) {
    Header h;
    std::memcpy(&h, c.bytes.data(), sizeof h);
    const std::uint32_t base = section == kFrames ? sizeof h : h.offset[section - 1];
    T out;
    std::memcpy(&out, c.bytes.data() + base + static_cast<std::size_t>(i) * sizeof(T), sizeof(T));
    return out;
}

Header header(const Converted& c) {
    Header h;
    std::memcpy(&h, c.bytes.data(), sizeof h);
    return h;
}

Export hero() {
    Export e = aseprite_fixture::row(4);
    e.frames[1] = {.x = 16, .y = 2, .w = 10, .h = 12, .ms = 50, .sx = 3, .sy = 4};
    e.tags = R"({"name": "idle", "from": 0, "to": 1, "direction": "forward"},
        {"name": "run", "from": 1, "to": 3, "direction": "pingpong_reverse", "repeat": "2"},
        {"name": "@step", "from": 1, "to": 2, "direction": "forward"},
        {"name": "@hit=2", "from": 2, "to": 2, "direction": "forward"})";
    e.slices = R"({"name": "pivot", "keys": [{"frame": 0, "bounds": {"x": 3, "y": 2, "w": 9, "h": 14}}]})";
    return e;
}

} // namespace

NNH_TEST(a_converted_export_is_a_valid_library_sized_to_its_contents) {
    Converted c;
    Problem p;
    CHECK(aseprite_fixture::convert(hero(), c, p));
    CHECK(nnh::detail::validate_sprite_library(c.bytes.data(), c.bytes.size()).ok());
    const Header h = header(c);
    CHECK(h.count[kFrames] == 4 && h.count[kClips] == 2 && h.count[kClipFrames] == 5 && h.count[kMarkers] == 4);
    for (int s = 0; s < kSectionCount; ++s) CHECK(h.cap[s] == h.count[s]);
    CHECK(c.dependencies.size() == 2 && c.dependencies[1] == "/g/assets/art/hero.png");
}

NNH_TEST(converted_clips_keep_modes_ends_and_durations) {
    Converted c;
    Problem p;
    CHECK(aseprite_fixture::convert(hero(), c, p));
    const ClipEntry idle = record<ClipEntry>(c, kClips, 0);
    const ClipEntry run = record<ClipEntry>(c, kClips, 1);
    CHECK(std::strcmp(idle.name, "idle") == 0 && idle.mode == kLoop && idle.length == 2);
    CHECK(idle.cycle_ticks == 24576 + 12288); // 100 ms + 50 ms
    CHECK(run.mode == kPingPong && run.direction == kReverse && run.count == 2 && run.first == 2 && run.length == 3);
    CHECK(run.end_cycle == 1 && run.end_step == 0); // two passes: 3 2 1 2 3, holds the frame it started on
}

NNH_TEST(marker_tags_reach_every_clip_frame_showing_their_frames) {
    Converted c;
    Problem p;
    CHECK(aseprite_fixture::convert(hero(), c, p));
    // idle shows frames 0 1; run shows 1 2 3. step covers 1-2, hit covers 2.
    const int expected[] = {0, 1, 1, 2, 0}; // markers per clip frame, in clip order
    int at = 0;
    for (int i = 0; i < 5; ++i) {
        const ClipFrameEntry f = record<ClipFrameEntry>(c, kClipFrames, i);
        CHECK(f.markers == expected[i] && f.first_marker == at);
        at += f.markers;
    }
    const MarkerEntry hit = record<MarkerEntry>(c, kMarkers, 3);
    CHECK(hit.hash == nnh::detail::fnv1a32("hit") && hit.value == 2);
}

NNH_TEST(converted_frames_carry_the_pivot_and_the_trim) {
    Converted c;
    Problem p;
    CHECK(aseprite_fixture::convert(hero(), c, p));
    const FrameEntry trimmed = record<FrameEntry>(c, kFrames, 1);
    CHECK(trimmed.x == 16 && trimmed.y == 2 && trimmed.w == 10 && trimmed.offset_x == 3 && trimmed.offset_y == 4);
    CHECK(trimmed.pivot_x == 120 && trimmed.pivot_y == 256); // (7.5, 16) in 1/16 px
    Export plain = aseprite_fixture::row(2);
    CHECK(aseprite_fixture::convert(plain, c, p));
    CHECK(record<FrameEntry>(c, kFrames, 0).pivot_x == 0 && record<ClipEntry>(c, kClips, 0).length == 2);
}

NNH_TEST(writer_limits_are_refused_with_the_line_at_fault) {
    Export crowded = aseprite_fixture::row(2);
    for (int i = 0; i < 9; ++i) {
        crowded.tags += std::string(i > 0 ? ",\n" : "") + "{\"name\": \"@m" + std::to_string(i) +
                        "\", \"from\": 0, \"to\": 0}";
    }
    Converted c;
    Problem p;
    CHECK(!aseprite_fixture::convert(crowded, c, p));
    CHECK(nnh::assetc::problem_text(p).find("hero.ase.json:16: more than 8 markers on frame 0") != std::string::npos);
    Export slow = aseprite_fixture::row(256);
    for (auto& f : slow.frames) f.ms = 65535;
    slow.tags = R"({"name": "slow", "from": 0, "to": 255, "direction": "pingpong"})";
    CHECK(!aseprite_fixture::convert(slow, c, p));
    CHECK(p.reason == "clip \"slow\" lasts over 1 hour" && p.line > 0);
    Export far = aseprite_fixture::row(1);
    far.slices = R"({"name": "pivot", "keys": [{"bounds": {"x": 3000, "y": 0, "w": 1, "h": 1}}]})";
    CHECK(!aseprite_fixture::convert(far, c, p));
    CHECK(p.reason.find("more than 2047 pixels") != std::string::npos);
}
