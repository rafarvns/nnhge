// The session's live value messages (spec 030 D51, D52): LiveSet and Live go to the live module when the game has one
// (refused with reason 7 otherwise, a Catalogue answered empty); Pause drives core's pause; LiveState records give the
// PC the pause state and flow control while frames stop. Debug/Development only (remote_session.cpp's apply_due).
// Tests: tests/remote/test_remote_live.cpp.

#include "app/dev_pause.hpp"
#include "debug/live_hook.hpp"
#include "remote/live_wire.hpp"
#include "remote/remote_internal.hpp"

namespace nnh::detail {

void remote_send_live_state(std::uint32_t next_frame, std::uint8_t flags) {
    const PauseState& p = dev_pause_state();
    std::uint8_t out[remote::kRecordHeaderSize + remote::kLiveStatePayload];
    remote::send_record(out, remote::encode_live_state(next_frame, p.paused(), flags, p.steps_left(), out));
}

void remote_live_message(const remote::Message& m, std::uint32_t frame) {
    using namespace remote;
    PauseState& p = dev_pause_state();
    if (m.type == MessageType::Pause) {
        switch (pause_op(m)) {
        case PauseOp::Pause: p.pause(); break;
        case PauseOp::Resume: p.resume(); break;
        case PauseOp::Step: p.step(pause_steps(m)); break;
        }
        remote_send_live_state(frame, 0);
        return;
    }
    const bool live = m.type == MessageType::LiveSet;
    const std::uint8_t op = live ? 0x40 : static_cast<std::uint8_t>(live_op(m)); // 0x40: live::kOpSet
    if (g_live_ops != nullptr) {
        g_live_ops->remote_op(op, live_arg(m), live_count(m), m.seq, live_hash(m), live_bits(m), frame, p.paused());
        if (!live && live_op(m) == LiveOp::Catalogue) remote_send_live_state(frame, kStateCatalogueComplete);
        return;
    }
    if (!live && live_op(m) == LiveOp::Catalogue) { // no live values: an empty catalogue
        remote_send_live_state(frame, kStateCatalogueComplete);
        return;
    }
    std::uint8_t out[kRecordHeaderSize + kLiveRefusedPayload];
    send_record(out, encode_live_refused(m.seq, live::Refusal::NoLiveValues, live_hash(m), out));
}

void remote_live_detached() {
    if (g_live_ops != nullptr) g_live_ops->remote_detached();
}

} // namespace nnh::detail
