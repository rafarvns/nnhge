// nnh::log — filtering, formatting and dispatch to the platform console, sinks and the ring.

#include "nnh/log.hpp"

#include <cstdarg>

#include "log/format.hpp"
#include "log/ring.hpp"
#include "nnh/app.hpp"
#include "nnh/platform/log.hpp"
#include "nnh/time.hpp"

namespace nnh::log {
namespace {

struct Sink {
    SinkFn fn = nullptr;
    void* user = nullptr;
};

Sink g_sinks[kMaxSinks];
detail::LogRing g_ring;
LogLevel g_global_level = LogLevel::Trace;
bool g_dispatching = false; // a sink that logs would recurse; drop instead

} // namespace

bool add_sink(SinkFn fn, void* user) {
    for (Sink& sink : g_sinks) {
        if (sink.fn == nullptr) {
            sink = Sink{fn, user};
            return true;
        }
    }
    return false;
}

void remove_sink(SinkFn fn, void* user) {
    for (Sink& sink : g_sinks) {
        if (sink.fn == fn && sink.user == user) sink = Sink{};
    }
}

void set_global_level(LogLevel level) { g_global_level = level; }

int ring_size() { return g_ring.size(); }
const char* ring_line(int index) { return g_ring.line(index); }

void reset() {
    for (Sink& sink : g_sinks) sink = Sink{};
    g_ring.clear();
    g_global_level = LogLevel::Trace;
}

void write(LogChannel& channel, LogLevel level, const char* format, ...) {
    if (level < channel.min_level || level < g_global_level || g_dispatching) return;
    detail::LogLine line;
    std::va_list args;
    va_start(args, format);
    detail::format_line_v(line, detail::LineHeader{frame_count(), time::now(), level, channel.name}, format,
                          args);
    va_end(args);

    g_dispatching = true;
    g_ring.push(line.text);
    platform::log_console(level, line.text);
    for (const Sink& sink : g_sinks) {
        if (sink.fn != nullptr) sink.fn(level, line.text, sink.user);
    }
    g_dispatching = false;
}

} // namespace nnh::log
