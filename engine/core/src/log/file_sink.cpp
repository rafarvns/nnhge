// log.txt sink (see file_sink.hpp). Writes go through the platform contract directly: the names are
// fixed and known to be valid, and a sink must not log.

#include "log/file_sink.hpp"

#include <cstring>

#include "nnh/log.hpp"
#include "nnh/platform/fs.hpp"

namespace nnh::detail {
namespace {

constexpr const char* kLog = "log.txt";
constexpr const char* kPrevious = "log.prev.txt";

void write_line(LogLevel, const char* line, void*) {
    char buffer[log::kMaxLineLength + 2];
    const std::size_t n = std::strlen(line);
    std::memcpy(buffer, line, n);
    buffer[n] = '\n';
    platform::fs_append(kLog, buffer, n + 1);
}

} // namespace

void log_file_start() {
    if (platform::fs_size(kLog) >= 0) {
        platform::fs_remove(kPrevious);
        platform::fs_rename(kLog, kPrevious);
    }
    log::add_sink(write_line, nullptr);
}

void log_file_stop() { log::remove_sink(write_line, nullptr); }

} // namespace nnh::detail
