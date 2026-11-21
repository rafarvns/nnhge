// Lines typed in nnh's terminal (remote/stdin_lines.hpp, spec 013 FR-5), read on a thread of their own. A
// temporary file stands in for the terminal.

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "remote/stdin_lines.hpp"
#include "test.hpp"

using nnh::tool::StdinLines;

namespace {

std::FILE* file_with(const std::string& text) {
    std::FILE* f = std::tmpfile();
    std::fwrite(text.data(), 1, text.size(), f);
    std::rewind(f);
    return f;
}

/// Takes lines until the input ended (at most ~2 s).
std::vector<std::string> take_all(StdinLines& typed) {
    std::vector<std::string> lines;
    for (int i = 0; i < 400 && !typed.ended(); ++i) {
        std::string line;
        while (typed.take(line)) lines.push_back(line);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return lines;
}

} // namespace

NNH_TEST(typed_lines_arrive_one_by_one_without_line_ends) {
    std::FILE* in = file_with("tap A\r\nscreenshot x\n\nexpect \"pressed\" 30\nend");
    StdinLines typed;
    std::string line;
    CHECK(!typed.take(line) && !typed.ended()); // nothing before start
    typed.start(in);
    const std::vector<std::string> lines = take_all(typed);
    CHECK(typed.ended());
    CHECK(lines.size() == 5);
    CHECK(lines.size() == 5 && lines[0] == "tap A" && lines[1] == "screenshot x" && lines[2].empty());
    CHECK(lines.size() == 5 && lines[3] == "expect \"pressed\" 30" && lines[4] == "end"); // last, unterminated
    CHECK(!typed.take(line));
    std::fclose(in);
}

NNH_TEST(a_long_typed_line_stays_one_line) {
    const std::string long_line(5000, 'x');
    std::FILE* in = file_with(long_line + "\nend\n");
    StdinLines typed;
    typed.start(in);
    const std::vector<std::string> lines = take_all(typed);
    CHECK(lines.size() == 2 && lines[0] == long_line && lines[1] == "end");
    std::fclose(in);
}

NNH_TEST(typed_input_that_ends_at_once_has_no_lines) {
    std::FILE* in = file_with("");
    StdinLines typed;
    typed.start(in);
    CHECK(take_all(typed).empty() && typed.ended());
    std::fclose(in);
}
