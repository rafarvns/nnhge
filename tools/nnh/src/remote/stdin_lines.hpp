#pragma once
// Lines typed in nnh's terminal while a game runs (spec 013 FR-5, interactive mode): read on a thread of their
// own so the session loop never waits for the keyboard. Tested in tools/nnh/tests/test_stdin_lines.cpp.

#include <cstdio>
#include <memory>
#include <string>

namespace nnh::tool {

class StdinLines {
public:
    StdinLines();
    ~StdinLines();
    StdinLines(const StdinLines&) = delete;
    StdinLines& operator=(const StdinLines&) = delete;

    /// Starts reading `in` (normally stdin) line by line. The thread is detached: a read blocked on the
    /// keyboard never holds up nnh's exit. Call once.
    void start(std::FILE* in);
    /// The next line, without '\r' and '\n'. @return false if no whole line is waiting.
    bool take(std::string& line);
    /// The input ended (end of file or an error) and every line was taken.
    bool ended() const;

    struct Shared; ///< the lines, shared with the reading thread (stdin_lines.cpp)

private:
    std::shared_ptr<Shared> shared_;
};

} // namespace nnh::tool
