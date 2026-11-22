// Clips (T2.2; FR-3, FR-5): names, frame lists, durations on the frame clock, modes and repeat values, capacity.

#include <cstring>

#include "library_fixture.hpp"

using nnh::Duration;
using nnh::FrameTime;
using nnh::Frames;
using nnh::SpriteClipId;
using nnh::SpriteLibrary;
using nnh::SpriteMode;

NNH_TEST(a_clip_from_a_range_has_its_name_length_and_duration) {
    SpriteLibrary lib = anim_test::strip(8);
    auto run = lib.add_clip({.name = "run", .first = 2, .count = 4, .frame_time = Frames{6}});
    CHECK(run.ok() && run.value().value == 0 && lib.clip_count() == 1);
    CHECK(std::strcmp(lib.clip_name(run.value()), "run") == 0);
    CHECK(lib.clip_length(run.value()) == 4);
    CHECK(lib.clip_duration(run.value()) == FrameTime{Frames{24}});
    nnh::SpriteAnimator a{lib};
    a.play(run.value());
    CHECK(a.frame() == 2);
}

NNH_TEST(a_clip_from_a_list_may_repeat_frames) {
    SpriteLibrary lib = anim_test::strip(4);
    const int frames[] = {3, 0, 3, 1};
    auto c = lib.add_clip({.name = "blink", .frames = frames, .count = 4});
    CHECK(c.ok());
    nnh::SpriteAnimator a{lib};
    a.play(c.value());
    int shown[4];
    shown[0] = a.frame();
    for (int i = 1; i < 4; ++i) {
        for (int f = 0; f < 6; ++f) a.update();
        shown[i] = a.frame();
    }
    CHECK(shown[0] == 3 && shown[1] == 0 && shown[2] == 3 && shown[3] == 1);
}

NNH_TEST(durations_are_rounded_once_to_the_frame_clock) {
    SpriteLibrary lib = anim_test::strip(4);
    const FrameTime each[] = {Duration::from_ms(100), Frames{2}, FrameTime::from_ticks(1), Duration::from_ms(1)};
    auto c = lib.add_clip({.name = "mixed", .count = 4, .durations = each});
    CHECK(c.ok());
    // 100 ms = 24 576 ticks, 2 frames = 8192, 1 tick, 1 ms = 246 ticks (245.76 rounded half up).
    CHECK(lib.clip_duration(c.value()).ticks == 24576 + 8192 + 1 + 246);
}

NNH_TEST(ping_pong_duration_is_one_round_trip) {
    SpriteLibrary lib = anim_test::strip(4);
    const FrameTime each[] = {Frames{1}, Frames{2}, Frames{3}, Frames{4}};
    auto c = lib.add_clip({.name = "pp", .count = 4, .durations = each, .mode = SpriteMode::PingPong});
    CHECK(lib.clip_duration(c.value()) == FrameTime{Frames{1 + 2 + 3 + 4 + 3 + 2}});
}

NNH_TEST(clip_names_are_checked) {
    SpriteLibrary lib = anim_test::strip(4);
    CHECK(!lib.add_clip({.name = nullptr, .count = 1}).ok());
    CHECK(!lib.add_clip({.name = "", .count = 1}).ok());
    CHECK(lib.add_clip({.name = "0123456789012345678901234567890", .count = 1}).ok()); // 31 bytes
    CHECK(!lib.add_clip({.name = "01234567890123456789012345678901", .count = 1}).ok());
    CHECK(!lib.add_clip({.name = "\xC3", .count = 1}).ok()); // cut UTF-8
    CHECK(lib.add_clip({.name = "idle", .count = 1}).ok());
    auto dup = lib.add_clip({.name = "idle", .count = 1});
    CHECK(!dup.ok() && std::strstr(dup.error().message, "idle") != nullptr);
}

NNH_TEST(clip_frames_and_durations_are_checked) {
    SpriteLibrary lib = anim_test::strip(4);
    CHECK(!lib.add_clip({.name = "a", .count = 0}).ok());
    CHECK(!lib.add_clip({.name = "a", .count = 257}).ok());
    CHECK(!lib.add_clip({.name = "a", .first = 2, .count = 3}).ok()); // frame 4 doesn't exist
    const int bad[] = {0, -1};
    CHECK(!lib.add_clip({.name = "a", .frames = bad, .count = 2}).ok());
    CHECK(!lib.add_clip({.name = "a", .count = 1, .frame_time = Frames{0}}).ok());
    CHECK(!lib.add_clip({.name = "a", .count = 1, .frame_time = Duration::from_ms(-5)}).ok());
    const FrameTime zero[] = {Frames{1}, Frames{0}};
    CHECK(!lib.add_clip({.name = "a", .count = 2, .durations = zero}).ok());
    CHECK(!lib.add_clip({.name = "a", .count = 1, .direction = static_cast<nnh::SpriteDirection>(4)}).ok());
    const FrameTime long_each[] = {Duration::from_seconds(3000), Duration::from_seconds(3000)};
    CHECK(!lib.add_clip({.name = "a", .count = 2, .durations = long_each}).ok()); // one play over 1 hour
    CHECK(lib.clip_count() == 0);
}

NNH_TEST(repeat_values_follow_the_mode) {
    SpriteLibrary lib = anim_test::strip(4);
    CHECK(!lib.add_clip({.name = "a", .count = 2, .mode = SpriteMode::Repeat}).ok()); // repeat 0
    CHECK(!lib.add_clip({.name = "a", .count = 2, .mode = SpriteMode::Repeat, .repeat = 65536}).ok());
    CHECK(!lib.add_clip({.name = "a", .count = 2, .mode = SpriteMode::Loop, .repeat = 2}).ok());
    CHECK(!lib.add_clip({.name = "a", .count = 2, .mode = SpriteMode::Once, .repeat = 2}).ok());
    CHECK(!lib.add_clip({.name = "a", .count = 2, .mode = SpriteMode::PingPong, .repeat = -1}).ok());
    CHECK(!lib.add_clip({.name = "a", .count = 2, .mode = SpriteMode::PingPong, .pingpong_passes = 70000}).ok());
    CHECK(!lib.add_clip({.name = "a", .count = 2, .mode = static_cast<SpriteMode>(9)}).ok());
    CHECK(lib.add_clip({.name = "r", .count = 2, .mode = SpriteMode::Repeat, .repeat = 65535}).ok());
    CHECK(lib.add_clip({.name = "o", .count = 2, .mode = SpriteMode::Once, .repeat = 1}).ok());
    CHECK(lib.add_clip({.name = "p", .count = 2, .mode = SpriteMode::PingPong, .pingpong_passes = 3}).ok());
}

NNH_TEST(clip_capacities_are_errors_not_asserts) {
    auto made = SpriteLibrary::create({.frames = 2, .clips = 2, .clip_frames = 3});
    SpriteLibrary lib = std::move(made.value());
    CHECK(lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 2, .columns = 2}).ok());
    CHECK(lib.add_clip({.name = "a", .count = 2}).ok());
    auto frames_full = lib.add_clip({.name = "b", .count = 2});
    CHECK(!frames_full.ok() && frames_full.error().code == nnh::ErrorCode::LimitReached);
    CHECK(lib.add_clip({.name = "b", .count = 1}).ok());
    auto clips_full = lib.add_clip({.name = "c", .count = 1});
    CHECK(!clips_full.ok() && clips_full.error().code == nnh::ErrorCode::LimitReached);
}

NNH_TEST(clip_queries_of_unknown_ids_answer_nothing) {
    SpriteLibrary lib = anim_test::strip(2);
    CHECK(std::strcmp(lib.clip_name({}), "") == 0);
    CHECK(lib.clip_length({3}) == 0);
    CHECK(lib.clip_duration({3}).ticks == 0);
    CHECK(!lib.find_clip(nullptr).valid());
}
