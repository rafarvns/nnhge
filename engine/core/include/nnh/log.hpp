#pragma once
/// @file nnh/log.hpp
/// Leveled, per-channel logging to pluggable sinks. See docs/api/log.md and
/// docs/architecture/logging.md.
///
/// @code
/// NNH_LOG_CHANNEL(player);                         // once, in one .cpp
/// NNH_LOG_INFO(player, "spawned at %d,%d", x, y);  // anywhere the channel is visible
/// @endcode

#include <cstddef>
#include <cstdint>

#include "nnh/build.hpp"
#include "nnh/detail/printf_format.hpp"

namespace nnh {

enum class LogLevel : std::uint8_t { Trace, Debug, Info, Warn, Error, Fatal, Off };

/// A named log channel with its own runtime minimum level. Declare with NNH_LOG_CHANNEL.
struct LogChannel {
    const char* name;
    LogLevel min_level = LogLevel::Trace;
};

namespace log {

inline constexpr std::size_t kMaxLineLength = 160; ///< bytes per formatted line, including '\0'
inline constexpr int kRingLines = 64;              ///< recent lines kept for the crash report
inline constexpr int kMaxSinks = 8;

/// Lowest level compiled in: Debug → Trace, Development → Debug, Release → Warn.
/// Log macros below it compile to nothing (no call, no format string in the binary).
inline constexpr LogLevel kCompiledMinLevel = build::flavor() == build::Flavor::Debug ? LogLevel::Trace
                                              : build::flavor() == build::Flavor::Development
                                                  ? LogLevel::Debug
                                                  : LogLevel::Warn;

/// Receives every formatted line that passes the filters. Must not log (it would be ignored).
using SinkFn = void (*)(LogLevel level, const char* line, void* user);

/// Registers a sink. @return false if all kMaxSinks slots are in use.
bool add_sink(SinkFn fn, void* user = nullptr);
void remove_sink(SinkFn fn, void* user = nullptr);

/// Messages below this level are dropped on every channel (default: Trace).
void set_global_level(LogLevel level);

/// Recent lines, oldest first (0 .. ring_size() - 1). Used by the crash report.
int ring_size();
const char* ring_line(int index);

/// Clears sinks, the ring buffer and the global level. For tests and engine restart.
void reset();

/// Use the NNH_LOG_* macros instead. Formats and dispatches one message.
void write(LogChannel& channel, LogLevel level, const char* format, ...) NNH_PRINTF_FORMAT(3, 4);

} // namespace log
} // namespace nnh

/// Defines a log channel (once per program, in a .cpp). Its name appears in every line.
#define NNH_LOG_CHANNEL(name) ::nnh::LogChannel nnh_log_channel_##name{#name}
/// Declares a channel defined in another file (put it in a header).
#define NNH_LOG_CHANNEL_EXTERN(name) extern ::nnh::LogChannel nnh_log_channel_##name
/// The channel object, e.g. to change its level: `NNH_LOG_CHANNEL_REF(player).min_level = ...`.
#define NNH_LOG_CHANNEL_REF(name) nnh_log_channel_##name

#if defined(NNH_FLAVOR_DEBUG)
#define NNH_LOG_MIN_LEVEL 0
#elif defined(NNH_FLAVOR_DEVELOPMENT)
#define NNH_LOG_MIN_LEVEL 1
#else
#define NNH_LOG_MIN_LEVEL 3
#endif

#define NNH_LOG_AT(level, channel, ...) ::nnh::log::write(NNH_LOG_CHANNEL_REF(channel), level, __VA_ARGS__)

#if NNH_LOG_MIN_LEVEL <= 0
#define NNH_LOG_TRACE(channel, ...) NNH_LOG_AT(::nnh::LogLevel::Trace, channel, __VA_ARGS__)
#else
#define NNH_LOG_TRACE(channel, ...) ((void)0)
#endif
#if NNH_LOG_MIN_LEVEL <= 1
#define NNH_LOG_DEBUG(channel, ...) NNH_LOG_AT(::nnh::LogLevel::Debug, channel, __VA_ARGS__)
#else
#define NNH_LOG_DEBUG(channel, ...) ((void)0)
#endif
#if NNH_LOG_MIN_LEVEL <= 2
#define NNH_LOG_INFO(channel, ...) NNH_LOG_AT(::nnh::LogLevel::Info, channel, __VA_ARGS__)
#else
#define NNH_LOG_INFO(channel, ...) ((void)0)
#endif
#define NNH_LOG_WARN(channel, ...) NNH_LOG_AT(::nnh::LogLevel::Warn, channel, __VA_ARGS__)
#define NNH_LOG_ERROR(channel, ...) NNH_LOG_AT(::nnh::LogLevel::Error, channel, __VA_ARGS__)
#define NNH_LOG_FATAL(channel, ...) NNH_LOG_AT(::nnh::LogLevel::Fatal, channel, __VA_ARGS__)
