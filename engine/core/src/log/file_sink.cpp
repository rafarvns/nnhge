// log.txt sink (see file_sink.hpp). Writes go through the platform contract directly: the names are
// fixed and known to be valid, and a sink must not log. Lines are batched in memory: one append to the 3DS SD card
// costs ~20 ms, a missed frame per line. A warning or worse writes the batch out at once (with the lines before it,
// in order), as do a full batch and log_file_stop(). The crash path logs a fatal line, so a crash flushes too.

#include "log/file_sink.hpp"

#include <cstring>

#include "nnh/log.hpp"
#include "nnh/platform/fs.hpp"

namespace nnh::detail {
namespace {

constexpr const char* kLog = "log.txt";
constexpr const char* kPrevious = "log.prev.txt";

constexpr std::size_t kBatchBytes = 4096; // ~40 typical lines per SD write
char g_batch[kBatchBytes];
std::size_t g_used = 0;

void flush() {
    if (g_used == 0) return;
    platform::fs_append(kLog, g_batch, g_used);
    g_used = 0;
}

void write_line(LogLevel level, const char* line, void*) {
    std::size_t n = std::strlen(line);
    if (n > log::kMaxLineLength) n = log::kMaxLineLength;
    if (g_used + n + 1 > kBatchBytes) flush();
    std::memcpy(g_batch + g_used, line, n);
    g_batch[g_used + n] = '\n';
    g_used += n + 1;
    if (level >= LogLevel::Warn) flush();
}

} // namespace

void log_file_start() {
    g_used = 0;
    if (platform::fs_size(kLog) >= 0) {
        platform::fs_remove(kPrevious);
        platform::fs_rename(kLog, kPrevious);
    }
    log::add_sink(write_line, nullptr);
}

void log_file_stop() {
    flush();
    log::remove_sink(write_line, nullptr);
}

} // namespace nnh::detail
