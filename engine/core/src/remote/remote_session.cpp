// Console remote session (see remote_internal.hpp): the PC's messages → schedule → remote input, and the
// records the console answers with. Fixed-size state (≈ 3.2 KB BSS, Debug/Development only), no heap.

#include "remote/remote_internal.hpp"

#include "log/engine_channels.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/remote.hpp"
#include "nnh/platform/system.hpp"
#include "nnh/profile.hpp"
#include "remote/remote_input.hpp"
#include "remote/remote_reader.hpp"
#include "remote/remote_schedule.hpp"
#include "remote/remote_wire.hpp"

namespace nnh::detail {
namespace {

using namespace remote;

/// Ticks go out on the Hello's frame and then every kTickEvery frames (plan 013, risks).
constexpr std::uint32_t kTickEvery = 4;
/// At most one "rejected" warning line per this many frames; the others are counted.
constexpr std::uint32_t kWarnEvery = 60;

struct Session {
    bool polling = true;    // the connection may exist; false after remote_receive() said -1
    bool attached = false;  // between Hello and Bye/off/disconnect: records may be sent
    bool quit = false;      // a Bye asked to quit
    bool exclusive = false; // a script owns the controls: the console's own input is ignored (Hello flag)
    bool check_rest = false; // a script just attached: warn about the console's controls not at rest
    bool off_logged = false;
    std::uint32_t next_tick = 0;
    bool warned = false;
    std::uint32_t last_warn = 0;
    int unreported = 0; // rejections since the last warning line
};

Session g_session;
RemoteReader g_reader;
RemoteSchedule g_schedule;
RemoteInput g_input;
std::uint8_t g_scratch[kScheduleCapacity * kMessageSize];
ReadItem g_items[kScheduleCapacity];

const char* reason_name(Reject reason) {
    switch (reason) {
    case Reject::None: break;
    case Reject::Magic: return "bad magic";
    case Reject::Type: return "unknown type";
    case Reject::Range: return "value out of range";
    case Reject::Order: return "frame out of order";
    case Reject::NotAttached: return "no Hello yet";
    case Reject::TooMany: return "remote input is off";
    }
    return "?";
}

/// Lets every remote control go and forgets what was waiting (Bye, off, disconnect).
void detach() {
    g_session.attached = false;
    g_session.exclusive = false;
    g_session.check_rest = false;
    g_schedule.clear();
    g_input.release_all();
    screenshots_clear();
}

void warn_rejected(std::uint16_t seq, Reject reason, std::uint32_t frame) {
    if (g_session.warned && frame - g_session.last_warn < kWarnEvery) {
        ++g_session.unreported;
        return;
    }
    if (g_session.unreported > 0) {
        NNH_LOG_WARN(remote, "rejected message %u: %s (%d more since the last warning)", seq, reason_name(reason),
                     g_session.unreported);
    } else {
        NNH_LOG_WARN(remote, "rejected message %u: %s", seq, reason_name(reason));
    }
    g_session.warned = true;
    g_session.last_warn = frame;
    g_session.unreported = 0;
}

void attach(std::uint32_t frame, bool exclusive) {
    if (!g_session.attached) {
        NNH_LOG_INFO(remote, "PC attached: remote control on%s",
                     exclusive ? " (script: only its input counts)" : "");
    }
    g_session.attached = true;
    g_session.exclusive = exclusive;
    g_session.check_rest = exclusive;
    std::uint8_t record[kMaxFixedRecord];
    send_record(record, encode_hello_record(frame, platform::capabilities(), build::flavor_name(), record));
    send_record(record, encode_tick_record(frame, record));
    g_session.next_tick = frame + kTickEvery;
}

void reject(const ReadItem& item, std::uint32_t frame) {
    std::uint8_t record[kMaxFixedRecord];
    send_record(record, encode_rejected_record(item.message.seq, item.reason, record));
    warn_rejected(item.message.seq, item.reason, frame);
    if (g_reader.off() && !g_session.off_logged) {
        g_session.off_logged = true;
        NNH_LOG_WARN(remote, "%d invalid messages in a row: remote input off until the PC reconnects",
                     kMaxBadInARow);
        detach();
    }
}

void accept(const Message& message, std::uint32_t frame) {
    if (message.type == MessageType::Hello) {
        attach(frame, message.exclusive);
    } else if (!g_schedule.push(message, frame)) {
        std::uint8_t record[kMaxFixedRecord];
        send_record(record, encode_dropped_record(message.seq, record));
    }
}

/// Reads what arrived (bounded by the queue's room, at least one message so a full queue answers Dropped).
/// @return false when the connection is gone.
bool read_messages(std::uint32_t frame) {
    NNH_PROFILE_SCOPE("remote"); // the per-frame poll's cost (a socket call on the console), in the overlay
    const std::size_t room = g_schedule.room() > 0 ? g_schedule.room() : 1;
    const long got = platform::remote_receive(g_scratch, g_reader.bytes_wanted(room));
    if (got < 0) return false;
    const FeedResult fed =
        g_reader.feed(g_scratch, static_cast<std::size_t>(got), frame, g_items, room);
    for (std::size_t i = 0; i < fed.items; ++i) {
        if (g_items[i].reason == Reject::None) accept(g_items[i].message, frame);
        else reject(g_items[i], frame);
    }
    return true;
}

void apply_due(std::uint32_t frame) {
    Message message;
    while (g_schedule.pop_due(frame, message)) {
        if (is_late(message, frame)) {
            std::uint8_t record[kMaxFixedRecord];
            send_record(record, encode_late_record(message.seq, message.frame, frame, record));
        }
        switch (message.type) {
        case MessageType::Screenshot: screenshots_request(message.id, message.screens, frame); break;
        case MessageType::Bye:
            NNH_LOG_INFO(remote, "PC detached: remote control off");
            g_session.quit = g_session.quit || message.quit;
            detach();
            return; // nothing after a Bye was queued by this connection
        default: g_input.apply(message); break;
        }
    }
}

/// The console's own controls a script hides, said once when it attaches (a resting Circle Pad past the dead
/// zone, like some Old 3DS pads, would otherwise have moved things: spec 013 amendment 2026-11-24).
void warn_not_at_rest(const platform::InputSample& physical) {
    const RestReport r = rest_report(physical);
    constexpr const char* kIgnored = "ignored while the script runs";
    if (r.circle_moved) {
        NNH_LOG_WARN(remote, "Circle Pad not at rest (%.3f, %.3f): %s", static_cast<double>(physical.circle_x),
                     static_cast<double>(physical.circle_y), kIgnored);
    }
    if (r.cstick_moved) {
        NNH_LOG_WARN(remote, "C-Stick not at rest (%.3f, %.3f): %s", static_cast<double>(physical.cstick_x),
                     static_cast<double>(physical.cstick_y), kIgnored);
    }
    if (r.buttons != 0) NNH_LOG_WARN(remote, "buttons held (0x%04X): %s", static_cast<unsigned>(r.buttons), kIgnored);
    if (r.touching) NNH_LOG_WARN(remote, "screen touched (%d, %d): %s", physical.touch_x, physical.touch_y, kIgnored);
}

void disconnected() {
    g_session.polling = false;
    if (g_session.attached) NNH_LOG_INFO(remote, "PC disconnected: remote controls released");
    detach();
}

} // namespace

bool remote::session_attached() { return g_session.attached; }

void remote::send_record(const std::uint8_t* bytes, std::size_t size) {
    if (g_session.attached) platform::remote_send(bytes, size);
}

bool remote_frame_start(std::uint64_t frame_number, platform::InputSample& sample) {
    const auto frame = static_cast<std::uint32_t>(frame_number);
    if (g_session.polling) {
        if (!read_messages(frame)) {
            disconnected();
        } else {
            apply_due(frame);
            if (g_session.attached && frame >= g_session.next_tick) {
                std::uint8_t record[kMaxFixedRecord];
                send_record(record, encode_tick_record(frame, record));
                g_session.next_tick = frame + kTickEvery;
            }
        }
    }
    if (g_session.check_rest) {
        g_session.check_rest = false;
        warn_not_at_rest(sample);
    }
    if (g_session.exclusive) g_input.replace_into(sample);
    else g_input.merge_into(sample);
    return !g_session.quit;
}

void remote_frame_ended(std::uint64_t frame) {
    if (g_session.attached) screenshots_send(static_cast<std::uint32_t>(frame));
}

void remote_reset() {
    g_session = Session{};
    g_reader.reset();
    g_schedule.clear();
    g_input.release_all();
    screenshots_clear();
}

void remote_shutdown() { screenshots_clear(); }

} // namespace nnh::detail
