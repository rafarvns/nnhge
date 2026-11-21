// Console remote schedule (spec 013 FR-1, FR-8): due exactly at its frame, FIFO, frame 0, late, full.

#include <cstdint>

#include "remote/remote_schedule.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;

namespace {

Message at(std::uint32_t frame, std::uint16_t seq = 0) {
    Message m;
    m.type = MessageType::Buttons;
    m.frame = frame;
    m.seq = seq;
    return m;
}

} // namespace

NNH_TEST(remote_schedule_starts_empty) {
    RemoteSchedule s;
    Message out;
    CHECK(s.size() == 0 && s.room() == kScheduleCapacity);
    CHECK(!s.pop_due(1000, out));
}

NNH_TEST(remote_schedule_a_message_is_due_at_its_frame_not_before) {
    RemoteSchedule s;
    CHECK(s.push(at(10, 1), 5));
    Message out;
    CHECK(!s.pop_due(9, out));
    CHECK(s.pop_due(10, out));
    CHECK(out.seq == 1 && out.frame == 10);
    CHECK(!s.pop_due(10, out)); // taken once
    CHECK(s.size() == 0);
}

NNH_TEST(remote_schedule_keeps_arrival_order_within_a_frame) {
    RemoteSchedule s;
    s.push(at(10, 1), 5);
    s.push(at(10, 2), 5);
    s.push(at(10, 3), 5);
    Message out;
    CHECK(s.pop_due(10, out) && out.seq == 1);
    CHECK(s.pop_due(10, out) && out.seq == 2);
    CHECK(s.pop_due(10, out) && out.seq == 3);
}

NNH_TEST(remote_schedule_gives_only_what_is_due) {
    RemoteSchedule s;
    s.push(at(10, 1), 5);
    s.push(at(12, 2), 5);
    Message out;
    CHECK(s.pop_due(11, out) && out.seq == 1);
    CHECK(!s.pop_due(11, out));
    CHECK(s.pop_due(12, out) && out.seq == 2);
}

NNH_TEST(remote_schedule_frame_0_is_due_at_the_frame_it_was_queued) {
    RemoteSchedule s;
    s.push(at(0, 4), 33);
    Message out;
    CHECK(!s.pop_due(32, out));
    CHECK(s.pop_due(33, out));
    CHECK(out.frame == 33);
    CHECK(!is_late(out, 33));
}

NNH_TEST(remote_schedule_a_late_message_is_applied_at_once_and_reported) {
    RemoteSchedule s;
    s.push(at(3, 9), 5);
    Message out;
    CHECK(s.pop_due(5, out));
    CHECK(is_late(out, 5));
    CHECK(out.frame == 3); // the frame it wanted
}

NNH_TEST(remote_schedule_on_time_is_not_late) {
    CHECK(!is_late(at(5), 5));
    CHECK(is_late(at(4), 5));
}

NNH_TEST(remote_schedule_refuses_when_full) {
    RemoteSchedule s;
    for (std::size_t i = 0; i < kScheduleCapacity; ++i) CHECK(s.push(at(10, static_cast<std::uint16_t>(i)), 5));
    CHECK(s.room() == 0);
    CHECK(!s.push(at(10, 999), 5));
    CHECK(s.size() == kScheduleCapacity);
    Message out;
    CHECK(s.pop_due(10, out) && out.seq == 0);
    CHECK(s.push(at(11, 1000), 5));
}

NNH_TEST(remote_schedule_keeps_order_across_the_ring_wrap) {
    RemoteSchedule s;
    Message out;
    std::uint16_t next_in = 0, next_out = 0;
    for (int round = 0; round < 5; ++round) {
        for (std::size_t n = s.room(); n > 0; --n) s.push(at(1, next_in++), 1);
        for (int i = 0; i < 40; ++i) {
            CHECK(s.pop_due(1, out));
            CHECK(out.seq == next_out++);
        }
    }
    for (std::size_t n = s.size(); n > 0; --n) CHECK(s.pop_due(1, out) && out.seq == next_out++);
    CHECK(next_out == next_in);
}

NNH_TEST(remote_schedule_clear_empties_it) {
    RemoteSchedule s;
    s.push(at(1), 1);
    s.push(at(2), 1);
    s.clear();
    Message out;
    CHECK(s.size() == 0 && !s.pop_due(100, out));
}
