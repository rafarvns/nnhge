#pragma once
// Readable game logs (spec 010 FR-5). Engine lines look like "[f12      0.200s] I app    message" (see
// engine/core/src/log/format.cpp); nnh colours the level, the channel and errors. Anything else passes
// through unchanged. Pure; unit tested in tools/nnh/tests/test_log_view.cpp.

#include <optional>
#include <string>

namespace nnh::tool {

struct LogLine {
    std::string stamp;   ///< "f12      0.200s"
    char level = 'I';    ///< T D I W E F
    std::string channel;
    std::string message;
};

/// The parts of an engine log line, or nullopt for any other text.
std::optional<LogLine> parse_log_line(const std::string& line);

/// The line as shown in a terminal: with ANSI colours when `colour`, else unchanged.
std::string render_log_line(const std::string& line, bool colour);

} // namespace nnh::tool
