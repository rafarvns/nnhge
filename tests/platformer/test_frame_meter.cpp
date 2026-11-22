// The platformer's 60 Hz meter (examples/platformer/frame_meter.hpp).

#include "platformer/frame_meter.hpp"
#include "test.hpp"

NNH_TEST(frame_meter_without_frames_is_not_ok) {
    FrameMeter meter;
    CHECK(!meter.ok());
    CHECK(meter.average_ms() == 0);
}

NNH_TEST(frame_meter_at_60_hz_is_ok) {
    FrameMeter meter;
    meter.add(16.7);
    meter.add(16.6);
    CHECK(meter.ok());
    CHECK(meter.frames() == 2);
    CHECK(meter.missed() == 0);
}

NNH_TEST(frame_meter_counts_a_frame_over_25_ms_as_missed) {
    FrameMeter meter;
    meter.add(16.7);
    meter.add(25.1);
    CHECK(meter.missed() == 1);
    CHECK(!meter.ok());
}

NNH_TEST(frame_meter_does_not_count_exactly_25_ms_as_missed) {
    FrameMeter meter;
    meter.add(25.0);
    CHECK(meter.missed() == 0);
}

NNH_TEST(frame_meter_keeps_the_average_and_the_worst_frame) {
    FrameMeter meter;
    meter.add(10);
    meter.add(30);
    meter.add(20);
    CHECK(meter.average_ms() == 20);
    CHECK(meter.worst_ms() == 30);
}

NNH_TEST(frame_meter_reset_forgets_everything) {
    FrameMeter meter;
    meter.add(40);
    meter.reset();
    CHECK(meter.frames() == 0);
    CHECK(meter.missed() == 0);
    CHECK(meter.worst_ms() == 0);
}
