// nnh::Frames and nnh::FrameTime: the 60 Hz frame clock in ticks of 1/4096 frame (spec 015 FR-16, plan 015
// "Clock"; declared in <nnh/time.hpp> as amended by plan 016).

#include "nnh/time.hpp"
#include "test.hpp"

using nnh::Duration;
using nnh::Frames;
using nnh::FrameTime;

NNH_TEST(frame_time_converts_whole_seconds_to_exact_frames) {
    CHECK(FrameTime(Duration::from_seconds(1)).ticks == 245'760);
    CHECK(FrameTime(Duration::from_ms(500)).ticks == 122'880);
    CHECK(FrameTime(Duration::from_ms(100)).ticks == 24'576);
    CHECK(FrameTime(Duration::from_seconds(1)).ticks == 60 * FrameTime::kTicksPerFrame);
}

NNH_TEST(frame_time_rounds_a_duration_to_the_nearest_tick) {
    CHECK(FrameTime(Duration::from_us(16'667)).ticks == 4096); // one frame, 16.67 ms
    CHECK(FrameTime(Duration{16'666'667}).ticks == 4096);
    CHECK(FrameTime(Duration::from_ms(1)).ticks == 246); // 245.76 rounds up
    CHECK(FrameTime(Duration{2034}).ticks == 0);         // 0.4999 tick
    CHECK(FrameTime(Duration{2035}).ticks == 1);         // 0.5001 tick
    CHECK(FrameTime(Duration{}).ticks == 0);
}

NNH_TEST(frame_time_from_frames_is_exact) {
    CHECK(FrameTime(Frames{1}).ticks == 4096);
    CHECK(FrameTime(Frames{30}).ticks == FrameTime(Duration::from_ms(500)).ticks);
    CHECK(FrameTime(Frames{0}).ticks == 0);
    CHECK(FrameTime(Frames{216'000}).ticks == nnh::kMaxFrameTime.ticks);
}

NNH_TEST(frame_time_marks_negative_and_over_an_hour_as_invalid) {
    CHECK(FrameTime(Duration{-1}).ticks == FrameTime::kInvalid);
    CHECK(!FrameTime(Duration::from_ms(-5)).valid());
    CHECK(FrameTime(Duration::from_seconds(3600)).valid());
    CHECK(FrameTime(Duration::from_seconds(3600)).ticks == 884'736'000);
    CHECK(!FrameTime(Duration::from_seconds(3600) + Duration{1}).valid());
    CHECK(!FrameTime(Duration::from_seconds(1'000'000'000)).valid());
    CHECK(!FrameTime(Frames{-1}).valid());
    CHECK(!FrameTime(Frames{216'001}).valid());
    CHECK(FrameTime{}.valid() && FrameTime{}.ticks == 0);
}

NNH_TEST(frame_time_converts_back_to_a_duration) {
    CHECK(FrameTime(Duration::from_seconds(1)).duration() == Duration::from_seconds(1));
    CHECK(FrameTime(Frames{1}).duration() == Duration{16'666'667});
    CHECK(FrameTime(Duration::from_ms(250)).duration() == Duration::from_ms(250));
    CHECK(nnh::kMaxFrameTime.duration() == Duration::from_seconds(3600));
    CHECK(FrameTime::from_ticks(1).duration() == Duration{4069}); // 4069.01 ns
    CHECK(FrameTime(Frames{3}).frames() == 3.0f);
    CHECK(FrameTime(Duration::from_ms(500)).frames() == 30.0f);
    CHECK(FrameTime::from_ticks(2048).frames() == 0.5f);
}

NNH_TEST(frame_time_is_usable_at_compile_time) {
    constexpr FrameTime half = Duration::from_ms(500);
    static_assert(half.ticks == 122'880);
    static_assert(FrameTime(Frames{2}) == FrameTime::from_ticks(8192));
    CHECK(half.valid());
}
