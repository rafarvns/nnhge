// Console remote session (spec 013 FR-1, FR-7, FR-8): the frame hooks over the fake platform's in-memory PC —
// attach on Hello, ticks, due messages, Late/Rejected/Dropped, warnings, disconnect, Bye and quit.

#include "nnh/build.hpp"
#include "nnh/input.hpp"
#include "nnh/platform/input.hpp"
#include "remote/remote_internal.hpp"
#include "remote_fixture.hpp"

using namespace remote_test;
using nnh::LogLevel;
using wire::RecordType;

namespace {

constexpr std::uint32_t kA = static_cast<std::uint32_t>(nnh::Button::A);
constexpr std::uint32_t kB = static_cast<std::uint32_t>(nnh::Button::B);

/// A fresh session with a PC connected.
void start() {
    fake::reset();
    nnh::detail::remote_reset();
    fake::remote_connect();
}

/// Runs the start of `frame` on an idle sample and returns what the game would read.
nnh::platform::InputSample at(std::uint64_t frame, bool* keep_running = nullptr) {
    nnh::platform::InputSample sample;
    const bool running = nnh::detail::remote_frame_start(frame, sample);
    if (keep_running != nullptr) *keep_running = running;
    return sample;
}

void run(std::uint64_t from, std::uint64_t to) {
    for (std::uint64_t f = from; f <= to; ++f) at(f);
}

std::uint32_t tick_frame(const Sent& s, int nth) {
    const Record* r = s.find(RecordType::Tick, nth);
    return r != nullptr ? le32(r->payload) : 0xFFFFFFFFu;
}

} // namespace

NNH_TEST(remote_session_without_a_connection_polls_once_and_stays_silent) {
    fake::reset(); // no remote_connect(): a game started without nnh
    nnh::detail::remote_reset();
    RemoteLines info(LogLevel::Info);
    run(1, 10);
    CHECK(fake::state().remote_receive_calls == 1); // -1 on the first frame: never asked again
    CHECK(info.count == 0);
}

NNH_TEST(remote_session_without_hello_sends_nothing) {
    start();
    RemoteLines warn(LogLevel::Warn);
    send(buttons(7, 0, kA));
    run(1, 12);
    CHECK(warn.count == 1); // logged on the console only
    std::size_t size = 99;
    fake::remote_sent(size);
    CHECK(size == 0); // no Rejected, no ticks: a PC that never says Hello sees plain text
    CHECK((at(13).buttons & kA) == 0);
}

NNH_TEST(remote_session_answers_hello_with_frame_capabilities_flavor_and_a_tick) {
    start();
    fake::set_capabilities({nnh::Capability::SecondScreen, nnh::Capability::Stereo3D});
    run(1, 2);
    send(hello());
    at(3);
    const Sent s = sent();
    CHECK(s.whole && s.count == 2);
    CHECK(s.records[0].type == RecordType::Hello);
    const std::uint8_t* p = s.records[0].payload;
    CHECK(le16(p) == wire::kVersion && le16(p + 2) == 0 && le32(p + 4) == 3);
    const std::uint32_t caps = (1u << static_cast<unsigned>(nnh::Capability::SecondScreen)) |
                               (1u << static_cast<unsigned>(nnh::Capability::Stereo3D));
    CHECK(le32(p + 8) == caps);
    CHECK(std::strncmp(reinterpret_cast<const char*>(p + 12), nnh::build::flavor_name(), 12) == 0);
    CHECK(s.records[1].type == RecordType::Tick && le32(s.records[1].payload) == 3);
}

NNH_TEST(remote_session_ticks_every_fourth_frame_after_hello) {
    start();
    send(hello());
    run(1, 12);
    const Sent s = sent();
    CHECK(s.count_of(RecordType::Tick) == 3);
    CHECK(tick_frame(s, 0) == 1 && tick_frame(s, 1) == 5 && tick_frame(s, 2) == 9);
}

NNH_TEST(remote_session_applies_a_message_on_its_frame_and_not_before) {
    start();
    send(hello());
    send(buttons(2, 5, kA));
    run(1, 3);
    CHECK((at(4).buttons & kA) == 0);
    CHECK((at(5).buttons & kA) != 0);
    CHECK((at(6).buttons & kA) != 0); // held until released
    CHECK(sent().count_of(RecordType::Late) == 0);
}

NNH_TEST(remote_session_merges_remote_buttons_with_the_real_ones) {
    start();
    send(hello());
    send(buttons(2, 0, kA));
    nnh::platform::InputSample sample;
    sample.buttons = kB;
    nnh::detail::remote_frame_start(1, sample);
    CHECK(sample.buttons == (kA | kB));
}

NNH_TEST(remote_session_applies_a_late_message_at_once_and_reports_it) {
    start();
    send(hello());
    run(1, 5);
    send(buttons(9, 4, kA)); // wanted frame 4, arrives at the start of frame 6
    CHECK((at(6).buttons & kA) != 0);
    const Sent s = sent();
    const Record* late = s.find(RecordType::Late);
    CHECK(late != nullptr && le16(late->payload) == 9 && le32(late->payload + 2) == 4 &&
          le32(late->payload + 6) == 6);
}

NNH_TEST(remote_session_answers_an_invalid_message_with_rejected_and_a_warning) {
    start();
    send(hello());
    at(1);
    RemoteLines warn(LogLevel::Warn);
    send_garbage(0x0405);
    at(2);
    const Sent s = sent();
    const Record* r = s.find(RecordType::Rejected);
    CHECK(r != nullptr && le16(r->payload) == 0x0405 && r->payload[2] == static_cast<std::uint8_t>(wire::Reject::Magic));
    CHECK(warn.count == 1);
}

NNH_TEST(remote_session_warns_at_most_once_per_60_frames_with_a_count) {
    start();
    send(hello());
    at(1);
    RemoteLines warn(LogLevel::Warn);
    for (std::uint64_t f = 2; f <= 61; ++f) {
        if (f % 10 == 2) send_garbage(static_cast<std::uint16_t>(f)); // frames 2, 12, … 52: six bad messages
        else if (f % 10 == 3) send(buttons(static_cast<std::uint16_t>(f), 0, 0)); // a good one between
        at(f);
    }
    CHECK(warn.count == 1);
    send_garbage(62);
    at(62);
    CHECK(warn.count == 2);
    CHECK(std::strstr(warn.last, "5 more") != nullptr); // frames 12 … 52 were counted, not printed
    CHECK(sent().count_of(RecordType::Rejected) == 7);  // every one is answered
}

NNH_TEST(remote_session_turns_off_after_eight_bad_messages_in_a_row) {
    start();
    send(hello());
    send(buttons(2, 0, kA));
    CHECK((at(1).buttons & kA) != 0);
    RemoteLines warn(LogLevel::Warn);
    for (int i = 0; i < 8; ++i) send_garbage(static_cast<std::uint16_t>(100 + i));
    CHECK((at(2).buttons & kA) == 0); // detached: every remote control released
    CHECK(std::strstr(warn.last, "off") != nullptr);
    fake::remote_clear_sent();
    send(buttons(3, 0, kA));
    run(3, 12);
    std::size_t size = 99;
    fake::remote_sent(size);
    CHECK(size == 0);                   // no more ticks or answers
    CHECK((at(13).buttons & kA) == 0); // and nothing applied
}

NNH_TEST(remote_session_reports_dropped_when_the_queue_is_full) {
    start();
    send(hello());
    for (int i = 0; i < 65; ++i) send(buttons(static_cast<std::uint16_t>(10 + i), 1000, kA));
    at(1); // Hello + 63 messages (64 read)
    CHECK(fake::state().remote_last_read == 64 * wire::kMessageSize);
    at(2); // 1 more: the queue is full
    CHECK(fake::state().remote_last_read == wire::kMessageSize);
    CHECK(sent().count_of(RecordType::Dropped) == 0);
    at(3); // still reads one message, which finds no room
    CHECK(fake::state().remote_last_read == wire::kMessageSize);
    const Sent s = sent();
    const Record* d = s.find(RecordType::Dropped);
    CHECK(d != nullptr && le16(d->payload) == 10 + 64);
}

NNH_TEST(remote_session_releases_everything_and_stops_polling_on_disconnect) {
    start();
    send(hello());
    send(buttons(2, 0, kA));
    send(stick(3, 0, 0, 1000, 0));
    send(touch(4, 0, true, 10, 20));
    const nnh::platform::InputSample held = at(1);
    CHECK((held.buttons & kA) != 0 && held.circle_x > 0.5f && held.touching);
    RemoteLines info(LogLevel::Info);
    fake::remote_disconnect();
    const nnh::platform::InputSample gone = at(2);
    CHECK(gone.buttons == 0 && gone.circle_x == 0.0f && !gone.touching);
    CHECK(info.count == 1);
    const int calls = fake::state().remote_receive_calls;
    run(3, 10);
    CHECK(fake::state().remote_receive_calls == calls);
}

NNH_TEST(remote_session_bye_releases_detaches_and_stops_ticks) {
    start();
    send(hello());
    send(buttons(2, 0, kA));
    send(bye(3, 3));
    CHECK((at(1).buttons & kA) != 0);
    CHECK((at(2).buttons & kA) != 0);
    bool running = false;
    CHECK((at(3, &running).buttons & kA) == 0);
    CHECK(running);
    fake::remote_clear_sent();
    send(buttons(4, 0, kA)); // after Bye: not attached
    run(4, 12);
    std::size_t size = 99;
    fake::remote_sent(size);
    CHECK(size == 0);
    CHECK((at(13).buttons & kA) == 0);
}

NNH_TEST(remote_session_bye_with_quit_ends_the_game_loop) {
    start();
    send(hello());
    send(bye(2, 4, true));
    bool running = false;
    at(3, &running);
    CHECK(running);
    at(4, &running);
    CHECK(!running);
    at(5, &running);
    CHECK(!running); // stays asked
    nnh::detail::remote_reset();
    fake::remote_connect();
    at(1, &running);
    CHECK(running); // a new engine run forgets it
}
