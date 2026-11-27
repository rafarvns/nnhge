#pragma once
// The apply point of hot reload (spec 031 D53, FR-6, FR-13, FR-15, Q10): a staged batch whose ReloadApply came due waits
// in `pending`; nnh::frame() calls g_reload_apply (non-null only then: one null check per frame) right after
// ++frame_count and before spec 030's live point. Paused ticks (D52) never reach that point, so a batch waits for the next
// game frame. Debug/Development only. Tests: tests/reload/test_reload_apply.cpp, test_reload_queries.cpp.

#include <cstddef>
#include <cstdint>

#include "reload/reload_hooks.hpp"
#include "reload/reload_wire.hpp"

namespace nnh::detail {


namespace reload {

class ReloadFiles;
class ReloadReceiver;

/// The session's received files and receiver (one per program).
ReloadFiles& session_files();
ReloadReceiver& session_receiver();

/// Registers the shutdown hook that frees and wipes everything (the first Begin of a program).
void ensure_shutdown_hook();
/// The receiver handed batch `batch` over (its Apply came due at `frame`): it applies at the next apply point.
void schedule_apply(std::uint16_t batch, std::uint16_t files);
/// A batch waits for its apply point.
bool apply_pending();
/// Applies the pending batch now (the body of g_reload_apply).
void apply_now(std::uint32_t frame);
/// Forgets a pending batch (the PC disconnected before its frame): its files are removed.
void drop_pending();

/// One warning line and a ReloadRefused record for a batch refused before its apply point (transfer, busy, notice).
void report_refusal(std::uint16_t batch, remote::ReloadStage stage, remote::ReloadReason reason, std::uint16_t file,
                    std::uint32_t frame, const char* what);

/// The records' gate (remote_reload.cpp): records go out only after the PC spoke reload in this attachment.
void send_reload_record(const std::uint8_t* bytes, std::size_t size);

/// Frees everything (the shutdown hook) and wipes the folder.
void reload_shutdown();

} // namespace reload
} // namespace nnh::detail
