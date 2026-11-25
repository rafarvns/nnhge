// Console remote screenshots (see remote_internal.hpp): asked for at the start of a frame, read back right after
// that frame's end_frame() and sent as they are (layout tagged; the PC converts). The one remote step allowed
// to block the frame (spec 013 FR-2); the frame is marked in the log so benchmarks can ignore it.

#include "remote/remote_internal.hpp"

#include "log/engine_channels.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/remote.hpp"
#include "nnh/platform/time.hpp"
#include "remote/remote_schedule.hpp"
#include "remote/remote_wire.hpp"

namespace nnh::detail::remote {
namespace {

struct Pending {
    std::uint16_t id = 0;
    std::uint8_t screens = 0;
    std::uint32_t frame = 0;
};

// At most one per queued message, so it never overflows (the schedule holds kScheduleCapacity).
Pending g_pending[kScheduleCapacity];
std::size_t g_pending_count = 0;

std::uint32_t bytes_per_pixel(platform::PixelLayout layout) {
    switch (layout) {
    case platform::PixelLayout::Rgba8Rows: return 4;
    case platform::PixelLayout::Bgr8ColumnsBottomUp: return 3;
    }
    return 0;
}

/// The platform's answer is one whole screen, in a known layout, that fits a record.
bool pixels_ok(const platform::ScreenPixels& px) {
    const std::uint32_t expected = std::uint32_t{px.width} * px.height * bytes_per_pixel(px.layout);
    return px.data != nullptr && expected != 0 && px.size == expected && px.size <= kMaxRecordPayload - kScreenFixed;
}

/// Screen record index 0 top, 1 bottom, 2 the top screen's right eye (spec 026).
void send_screen(std::uint16_t id, std::uint8_t index, std::uint32_t frame) {
    static constexpr const char* kNames[3] = {"top", "bottom", "top (right eye)"};
    std::uint8_t record[kMaxFixedRecord];
    platform::ScreenPixels px;
    const bool read = index == 2 ? platform::remote_capture_right(px)
                                 : platform::remote_capture(index == 0 ? Screen::Top : Screen::Bottom, px);
    if (!read || !pixels_ok(px)) {
        NNH_LOG_WARN(remote, "screenshot %u: the %s screen could not be read back (%s)", id, kNames[index],
                     platform::last_error());
        send_record(record, encode_screen_failed_record(id, index, frame, record));
        return;
    }
    ScreenHeader h;
    h.id = id;
    h.screen = index;
    h.layout = static_cast<std::uint8_t>(px.layout);
    h.frame = frame;
    h.width = px.width;
    h.height = px.height;
    send_record(record, encode_screen_record(h, px.size, record));
    send_record(px.data, px.size);
}

} // namespace

void screenshots_request(std::uint16_t id, std::uint8_t screens, std::uint32_t frame) {
    if (g_pending_count < kScheduleCapacity) g_pending[g_pending_count++] = {id, screens, frame};
}

void screenshots_send(std::uint32_t frame) {
    if (g_pending_count == 0) return;
    const std::int64_t started = platform::ticks_ns();
    std::size_t kept = 0;
    for (std::size_t i = 0; i < g_pending_count; ++i) {
        const Pending p = g_pending[i];
        if (p.frame > frame) {
            g_pending[kept++] = p;
            continue;
        }
        if ((p.screens & kScreenTop) != 0) send_screen(p.id, 0, frame);
        if ((p.screens & kScreenTopRight) != 0) send_screen(p.id, 2, frame);
        if ((p.screens & kScreenBottom) != 0) send_screen(p.id, 1, frame);
    }
    g_pending_count = kept;
    // Unused where NNH_LOG_INFO compiles out (Release never calls this file, but it is still compiled).
    [[maybe_unused]] const auto ms = static_cast<unsigned long>((platform::ticks_ns() - started) / 1000000);
    NNH_LOG_INFO(remote, "frame %lu sent a screenshot in %lu ms; its time is not representative",
                 static_cast<unsigned long>(frame), ms);
}

void screenshots_clear() {
    g_pending_count = 0;
    platform::remote_capture_release();
}

} // namespace nnh::detail::remote
