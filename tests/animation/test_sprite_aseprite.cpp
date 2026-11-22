// A real conversion end to end (spec 016 T7.4, FR-16, FR-17): tests/data/aseprite/good.ase.json converted by
// nnh-assetc at build time is loaded by the engine and plays the frames, durations, markers and pivot it describes.

#include <cstdio>
#include <vector>

#include "animation/sprite_load.hpp"
#include "library_fixture.hpp"

using nnh::SpriteAnimator;
using nnh::SpriteLibrary;

namespace {

SpriteLibrary converted_good() {
    std::vector<std::uint8_t> bytes;
    if (std::FILE* f = std::fopen(NNH_TEST_SPRITES_DIR "/good.nnhsprite", "rb")) {
        std::uint8_t buffer[4096];
        std::size_t got = 0;
        while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0) bytes.insert(bytes.end(), buffer, buffer + got);
        std::fclose(f);
    }
    auto lib = nnh::detail::sprite_library_from_file(bytes.data(), bytes.size(), {}, "good");
    CHECK(lib.ok());
    return lib.ok() ? std::move(lib.value()) : SpriteLibrary{};
}

} // namespace

NNH_TEST(a_converted_aseprite_export_has_its_tags_as_clips) {
    const SpriteLibrary lib = converted_good();
    CHECK(lib.frame_count() == 4 && lib.clip_count() == 5);
    CHECK(lib.find_clip("idle").value == 0 && lib.find_clip("once").value == 4);
    CHECK(!lib.find_clip("@step").valid()); // marker tags are not clips
    // idle: 100 ms + 50 ms; run (ping-pong over 1..3): one round trip 50 + 70 + 100 + 70 ms.
    CHECK(lib.clip_duration(lib.find_clip("idle")).ticks == 24576 + 12288);
    CHECK(lib.clip_duration(lib.find_clip("run")).ticks == 12288 + 17203 + 24576 + 17203);
}

NNH_TEST(a_converted_export_plays_with_its_markers_and_pivot) {
    const SpriteLibrary lib = converted_good();
    SpriteAnimator a{lib};
    a.play(lib.find_clip("run")); // pingpong_reverse, repeat 2: 3 2 1 2 3, then holds 3
    CHECK(a.frame() == 3);
    const nnh::SpriteNotices first = a.update();
    CHECK(first.count == 1 && first.data[0].marker == nnh::sprite_name("hit") && first.data[0].value == 2);
    int steps = 0;
    for (int i = 0; i < 40; ++i) {
        for (const nnh::SpriteNotice& n : a.update()) steps += n.marker == nnh::sprite_name("step") ? 1 : 0;
    }
    CHECK(steps == 3 && a.finished() && a.frame() == 3);
    a.play(lib.find_clip("idle"));
    a.update();
    for (int i = 0; i < 5; ++i) a.update();
    CHECK(a.frame() == 1);                              // the trimmed frame
    CHECK(a.offset() == (nnh::Vec2{3, 4}));
    CHECK(a.pivot() == (nnh::Vec2{5, 12}));             // slice pivot (8, 16) minus the trim offset
    CHECK(lib.frame(0).pivot == (nnh::Vec2{8, 16}));    // every frame takes the slice's pivot
}
