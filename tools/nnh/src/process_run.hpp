#pragma once
// Running a program and reading what it says (spec 033 FR-18, D56 item 5): no shell (the arguments go to the
// program as they are), stdout and stderr merged, a time limit after which the program is killed, and a bound
// on the output kept (the rest is read and dropped, so the program never blocks on a full pipe). `nnh doctor`
// reads tool versions this way. Windows: CreateProcessW with each argument quoted by the C runtime's rules;
// elsewhere posix_spawnp and poll. Tested in tools/nnh/tests/test_process_run.cpp.

#include <cstddef>
#include <string>
#include <vector>

namespace nnh::tool {

struct RunResult {
    bool started = false;   ///< false: the program could not be started (missing, not executable)
    int exit_code = -1;     ///< its exit status (-1 when killed or not started)
    bool timed_out = false; ///< killed at the time limit
    std::string output;     ///< stdout and stderr, at most `max_output` bytes, '\r' kept as written
};

/// Runs `program` (a path, or a name looked up on PATH) with `args`, waits at most `timeout_ms`.
RunResult run_program(const std::string& program, const std::vector<std::string>& args, int timeout_ms,
                      std::size_t max_output);

/// The command line Windows' C runtime splits back into `args` (exposed for tests).
std::string windows_command_line(const std::string& program, const std::vector<std::string>& args);

} // namespace nnh::tool
