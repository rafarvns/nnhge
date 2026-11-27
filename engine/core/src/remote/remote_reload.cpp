// The session's hot reload messages (spec 031 D54): transfer messages and their payload at receipt (the receiver writes
// the files), ReloadApply from the schedule (the batch waits for its apply point), notices from `nnh`, and the records'
// gate: ReloadStaged/Applied/Refused go out only after the PC sent a ReloadBegin or ReloadNotice in this attachment.
// Debug/Development only (remote_session.cpp). Tests: tests/reload/test_reload_apply.cpp.

#include "remote/remote_reload.hpp"

#include "reload/reload_apply.hpp"
#include "reload/reload_files.hpp"
#include "reload/reload_hooks.hpp"
#include "reload/reload_receiver.hpp"
#include "remote/remote_internal.hpp"

namespace nnh::detail {
namespace {

using namespace remote;
using reload::ReceiverEvent;

bool g_spoken = false; // the PC spoke reload in this attachment: records may go out

struct Notice {
    bool collecting = false;
    NoticeReason reason = NoticeReason::ConversionFailed;
    std::uint8_t length = 0, got = 0;
    char path[kReloadMaxPath + 1] = {};
};
Notice g_notice;

void handle(ReceiverEvent e, std::uint32_t frame) {
    reload::ReloadReceiver& r = reload::session_receiver();
    switch (e) {
    case ReceiverEvent::None: return;
    case ReceiverEvent::Staged: {
        std::uint8_t out[kMaxReloadRecord];
        reload::send_reload_record(out, encode_reload_staged({r.batch(), r.files(), frame}, out));
        return;
    }
    case ReceiverEvent::Refused:
        reload::report_refusal(r.refusal().batch, ReloadStage::Transfer, r.refusal().reason, r.refusal().file, frame,
                               "transfer");
        return;
    case ReceiverEvent::Cancelled:
        reload::report_refusal(r.batch(), ReloadStage::Cancelled, ReloadReason::None, kNoFile, frame, "cancelled");
        return;
    }
}

void begin(const Message& m, std::uint32_t frame) {
    reload::ReloadFiles& files = reload::session_files();
    reload::ensure_shutdown_hook();
    if (reload::apply_pending()) { // a batch waits for its frame: one at a time
        reload::report_refusal(reload_batch(m), ReloadStage::Busy, ReloadReason::Batch, kNoFile, frame, "busy");
        return;
    }
    if (!files.ready()) {
        reload::report_refusal(reload_batch(m), ReloadStage::Memory, ReloadReason::NoMemory, kNoFile, frame,
                               "receive buffer");
        return;
    }
    reload::ReloadReceiver& r = reload::session_receiver();
    if (r.on_message(m, files.sink()) == ReceiverEvent::Refused) {
        reload::report_refusal(reload_batch(m), ReloadStage::Busy, ReloadReason::Batch, kNoFile, frame, "busy");
        return;
    }
    if (!files.begin_batch(reload_files(m), reload_total(m))) {
        r.detach(files.sink());
        reload::report_refusal(reload_batch(m), ReloadStage::Memory, ReloadReason::NoMemory, kNoFile, frame,
                               "batch block");
    }
}

void notice_done(std::uint32_t frame) {
    g_notice.collecting = false;
    g_notice.path[g_notice.got] = '\0';
    const bool restart = g_notice.reason == NoticeReason::NeedsRestart;
    reload::report_refusal(0, restart ? ReloadStage::Restart : ReloadStage::Notice,
                           restart ? ReloadReason::NeedsRestart : ReloadReason::ConversionFailed, kNoFile, frame,
                           g_notice.path);
}

} // namespace

void remote_reload_message(const Message& m, std::uint32_t frame) {
    if (m.type == MessageType::ReloadBegin || m.type == MessageType::ReloadNotice) g_spoken = true;
    switch (m.type) {
    case MessageType::ReloadBegin: begin(m, frame); return;
    case MessageType::ReloadNotice: {
        Notice n;
        n.collecting = true;
        n.reason = notice_reason(m);
        n.length = reload_path_length(m);
        g_notice = n;
        return;
    }
    default: handle(reload::session_receiver().on_message(m, reload::session_files().sink()), frame); return;
    }
}

void remote_reload_payload(const std::uint8_t* data, std::size_t size, std::uint32_t frame) {
    if (g_notice.collecting) {
        for (std::size_t i = 0; i < size && g_notice.got < g_notice.length; ++i) {
            const char c = static_cast<char>(data[i]);
            g_notice.path[g_notice.got++] = c >= 0x20 && c < 0x7F ? c : '?'; // printed only: never a file name
        }
        if (g_notice.got == g_notice.length) notice_done(frame);
        return;
    }
    handle(reload::session_receiver().on_payload(data, size, reload::session_files().sink()), frame);
}

void remote_reload_apply(const Message& m, std::uint32_t frame) {
    g_spoken = true;
    reload::ReloadReceiver& r = reload::session_receiver();
    if (r.take(reload_batch(m), reload_files(m))) {
        reload::schedule_apply(reload_batch(m), reload_files(m));
        return;
    }
    reload::report_refusal(reload_batch(m), ReloadStage::Transfer, ReloadReason::Batch, kNoFile, frame, "apply");
}

std::uint8_t* remote_reload_window(std::size_t& room) {
    room = 0;
    if (reload::session_receiver().state() != reload::ReloadReceiver::State::Open) return nullptr;
    return reload::session_files().window(room);
}

void remote_reload_flush() {
    if (reload::session_receiver().state() == reload::ReloadReceiver::State::Open) reload::session_files().flush();
}

void remote_reload_detached() {
    reload::session_receiver().detach(reload::session_files().sink()); // the PC is gone: nothing to tell
    reload::drop_pending();
    g_notice = Notice{};
    g_spoken = false;
}

void reload::send_reload_record(const std::uint8_t* bytes, std::size_t size) {
    if (g_spoken) remote::send_record(bytes, size);
}

const ReloadSessionOps kReloadSessionOps{remote_reload_message, remote_reload_payload, remote_reload_apply,
                                         remote_reload_window,  remote_reload_flush,   remote_reload_detached};

} // namespace nnh::detail
