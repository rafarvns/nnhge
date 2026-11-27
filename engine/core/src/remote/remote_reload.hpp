#pragma once
// The session's hot reload hooks (spec 031 D54), called by remote_session.cpp. Debug/Development only.
// Tests: tests/reload/test_reload_apply.cpp.

#include <cstddef>
#include <cstdint>

#include "remote/remote_wire.hpp"

namespace nnh::detail {

/// Payload bytes the session reads per frame at most (plan 031 question 10).
inline constexpr std::size_t kReloadReadPerFrame = 16 * 1024;

/// An accepted transfer message (Begin, File, Chunk, Cancel, Notice), handled at receipt.
void remote_reload_message(const remote::Message& m, std::uint32_t frame);
/// A piece of the payload of the last transfer message.
void remote_reload_payload(const std::uint8_t* data, std::size_t size, std::uint32_t frame);
/// A ReloadApply that came due at `frame` (from the schedule).
void remote_reload_apply(const remote::Message& m, std::uint32_t frame);
/// Where payload bytes may be read in place (null: read them into the session's scratch).
std::uint8_t* remote_reload_window(std::size_t& room);
/// The end of a frame's reading: buffered bytes reach the open file.
void remote_reload_flush();
/// The PC detached or disconnected: the open, staged or pending batch is dropped (its files removed).
void remote_reload_detached();

struct ReloadSessionOps;
/// The functions above, as g_reload_session points at them once a kind is registered (reload_hooks.hpp).
extern const ReloadSessionOps kReloadSessionOps;

} // namespace nnh::detail
