#pragma once
// Console remote session (spec 013, D28): the hooks nnh::frame() calls, and what the session and the
// screenshots share. Called from app.cpp only when build::kDevToolsEnabled, so Release links none of it.
// Protocol and timing: specs/013-console-remote/plan.md; overview in docs/architecture/debugging.md ("Driving a
// game from the PC"). Tests: tests/remote/test_remote_{session,frames,screenshot}.cpp.

#include <cstddef>
#include <cstdint>

#include "nnh/platform/input.hpp"

namespace nnh::detail {

/// Start of frame `frame` (= frame_count() during it), between platform::read_input() and input_update():
/// reads what the PC sent (non-blocking, at most what the queue has room for, at least one message), answers
/// Hello, applies the messages due, merges the remote controls into `sample` — or, while a script is attached
/// (Hello flag kHelloExclusive), replaces `sample` with them, after warning once about console controls not at
/// rest — and sends a tick when due. After the connection is gone it only merges (released) controls. Never
/// allocates. @return false once the PC asked to quit (Bye with quit).
bool remote_frame_start(std::uint64_t frame, platform::InputSample& sample);

/// Right after platform::end_frame() of `frame`: sends the screenshots due for it (the only remote work that
/// may block the frame).
void remote_frame_ended(std::uint64_t frame);

/// Forgets every session state (nnh::init()): the next frame polls for a PC again.
void remote_reset();

/// Frees the screenshot buffer (nnh::shutdown(), before platform::shutdown()).
void remote_shutdown();

namespace remote {

/// True between the PC's Hello and its Bye (or the remote going off, or the connection closing): records
/// may be sent only then.
bool session_attached();

/// Sends one record (header included) if attached. Never allocates.
void send_record(const std::uint8_t* bytes, std::size_t size);

/// Screenshots (remote_screenshot.cpp): asked for at the start of `frame`, taken at its end.
void screenshots_request(std::uint16_t id, std::uint8_t screens, std::uint32_t frame);
/// Sends the Screen/ScreenFailed records of every screenshot asked for `frame` (or earlier).
void screenshots_send(std::uint32_t frame);
/// Forgets the screenshots asked for and frees the read-back buffer (Bye, disconnect, shutdown).
void screenshots_clear();

} // namespace remote
} // namespace nnh::detail
