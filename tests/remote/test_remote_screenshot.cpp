// Console remote screenshots (spec 013 FR-2): taken at the end of the frame they were asked for, only the
// screens asked, sent as Screen records (or ScreenFailed), the slow frame marked in the log, and the read-back
// buffer freed when the PC leaves.

#include "nnh/app.hpp"
#include "remote_fixture.hpp"

using namespace remote_test;
using nnh::LogLevel;
using wire::RecordType;

namespace {

void start_attached() {
    nnh::shutdown();
    fake::reset();
    fake::remote_connect();
    CHECK(nnh::init());
    send(hello());
}

void frames(int count) {
    for (int i = 0; i < count; ++i) CHECK(nnh::frame());
}

bool pixel_is(const std::uint8_t* pixels, nnh::Screen screen, int x, int y) {
    std::uint8_t want[4];
    fake::remote_capture_pixel(screen, x, y, want);
    const std::uint8_t* got = pixels + (y * nnh::screen_width(screen) + x) * 4;
    return std::memcmp(got, want, 4) == 0;
}

} // namespace

NNH_TEST(remote_screenshot_is_taken_at_the_end_of_its_frame) {
    start_attached();
    send(screenshot(2, 5, 0x0102, wire::kScreenTop | wire::kScreenBottom));
    frames(5);
    CHECK(sent().count_of(RecordType::Screen) == 0); // frame 5 is still being drawn
    CHECK(nnh::frame());                             // ends frame 5
    const Sent s = sent();
    CHECK(s.whole && s.count_of(RecordType::Screen) == 2);
    const Record* top = s.find(RecordType::Screen, 0);
    const Record* bottom = s.find(RecordType::Screen, 1);
    CHECK(top != nullptr && bottom != nullptr);
    const std::uint8_t* t = top->payload;
    CHECK(le16(t) == 0x0102 && t[2] == 0 && t[3] == 1 && le32(t + 4) == 5 && le16(t + 8) == 400 && le16(t + 10) == 240);
    CHECK(top->length == wire::kScreenFixed + 400u * 240u * 4u);
    CHECK(pixel_is(t + 12, nnh::Screen::Top, 0, 0) && pixel_is(t + 12, nnh::Screen::Top, 399, 239));
    const std::uint8_t* b = bottom->payload;
    CHECK(le16(b) == 0x0102 && b[2] == 1 && le32(b + 4) == 5 && le16(b + 8) == 320 && le16(b + 10) == 240);
    CHECK(bottom->length == wire::kScreenFixed + 320u * 240u * 4u);
    CHECK(pixel_is(b + 12, nnh::Screen::Bottom, 319, 0) && pixel_is(b + 12, nnh::Screen::Bottom, 17, 200));
    nnh::shutdown();
}

NNH_TEST(remote_screenshot_reads_back_only_the_screens_asked) {
    start_attached();
    send(screenshot(2, 1, 9, wire::kScreenBottom));
    frames(2);
    const Sent s = sent();
    CHECK(s.count_of(RecordType::Screen) == 1 && s.find(RecordType::Screen)->payload[2] == 1);
    CHECK(fake::state().remote_captures == 1);
    nnh::shutdown();
}

NNH_TEST(remote_screenshot_that_fails_sends_screen_failed_for_each_screen) {
    start_attached();
    fake::set_remote_capture_fails(true);
    send(screenshot(2, 2, 0x0A0B, wire::kScreenTop | wire::kScreenBottom));
    RemoteLines warn(LogLevel::Warn);
    frames(3);
    const Sent s = sent();
    CHECK(s.count_of(RecordType::Screen) == 0 && s.count_of(RecordType::ScreenFailed) == 2);
    const Record* f = s.find(RecordType::ScreenFailed, 1);
    CHECK(f != nullptr && le16(f->payload) == 0x0A0B && f->payload[2] == 1 && le32(f->payload + 3) == 2);
    CHECK(warn.count >= 1);
    nnh::shutdown();
}

NNH_TEST(remote_screenshot_frame_is_marked_in_the_log) {
    start_attached();
    send(screenshot(2, 4, 1, wire::kScreenTop));
    RemoteLines info(LogLevel::Info);
    frames(5);
    CHECK(info.count >= 1);
    CHECK(std::strncmp(info.last, "[f4 ", 4) == 0); // logged in the frame it describes
    CHECK(std::strstr(info.last, "not representative") != nullptr);
    nnh::shutdown();
}

NNH_TEST(remote_screenshot_asked_late_is_taken_at_the_end_of_the_frame_it_arrives) {
    start_attached();
    frames(5);
    send(screenshot(2, 3, 1, wire::kScreenTop));
    frames(2); // arrives at frame 6, ends with it
    const Sent s = sent();
    const Record* r = s.find(RecordType::Screen);
    CHECK(r != nullptr && le32(r->payload + 4) == 6);
    CHECK(s.count_of(RecordType::Late) == 1);
    nnh::shutdown();
}

NNH_TEST(remote_screenshot_buffer_is_freed_at_bye) {
    start_attached();
    send(screenshot(2, 1, 1, wire::kScreenTop));
    send(bye(3, 3));
    frames(2);
    CHECK(fake::state().remote_capture_held);
    frames(1);
    CHECK(!fake::state().remote_capture_held && fake::state().remote_capture_releases == 1);
    nnh::shutdown();
}

NNH_TEST(remote_screenshot_buffer_is_freed_at_disconnect) {
    start_attached();
    send(screenshot(2, 1, 1, wire::kScreenTop));
    frames(2);
    CHECK(fake::state().remote_capture_held);
    fake::remote_disconnect();
    frames(1);
    CHECK(!fake::state().remote_capture_held);
    nnh::shutdown();
}

NNH_TEST(remote_screenshot_buffer_is_freed_at_shutdown) {
    start_attached();
    send(screenshot(2, 1, 1, wire::kScreenTop));
    frames(2);
    CHECK(fake::state().remote_capture_held);
    nnh::shutdown();
    CHECK(!fake::state().remote_capture_held);
}

NNH_TEST(remote_screenshot_without_a_pc_is_never_taken) {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
    frames(5);
    nnh::shutdown();
    CHECK(fake::state().remote_captures == 0 && fake::state().remote_capture_releases == 0);
}
