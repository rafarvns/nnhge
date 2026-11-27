// Running programs with a time limit and bounded output (process_run.hpp, spec 033 FR-18). CMake is the helper program:
// it is on every machine that builds nnh (NNH_TOOL_TEST_CMAKE).

#include <chrono>
#include <fstream>
#include <string>

#include "doctor/probes.hpp"
#include "doctor/tool_versions.hpp"
#include "process_run.hpp"
#include "test.hpp"

using namespace nnh::tool;

NNH_TEST(a_program_runs_with_its_arguments_and_exit_code) {
    RunResult r = run_program(NNH_TOOL_TEST_CMAKE, {"-E", "echo", "hello world", "a\"b"}, 10000, 4096);
    CHECK(r.started && !r.timed_out && r.exit_code == 0);
    CHECK(r.output.find("hello world a\"b") != std::string::npos);
    r = run_program(NNH_TOOL_TEST_CMAKE, {"-E", "false"}, 10000, 4096);
    CHECK(r.started && r.exit_code == 1);
}

NNH_TEST(stdout_and_stderr_are_both_read) {
    const std::string script = std::string(NNH_TOOL_TEST_TMP) + "/process_run_stderr.cmake";
    std::ofstream(script) << "message(\"to stderr\")\nexecute_process(COMMAND ${CMAKE_COMMAND} -E echo to stdout)\n";
    const RunResult r = run_program(NNH_TOOL_TEST_CMAKE, {"-P", script}, 10000, 4096);
    CHECK(r.started && r.exit_code == 0);
    CHECK(r.output.find("to stderr") != std::string::npos && r.output.find("to stdout") != std::string::npos);
}

NNH_TEST(a_program_past_its_time_limit_is_killed) {
    const auto start = std::chrono::steady_clock::now();
    const RunResult r = run_program(NNH_TOOL_TEST_CMAKE, {"-E", "sleep", "20"}, 300, 4096);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    CHECK(r.started && r.timed_out && r.exit_code == -1);
    CHECK(ms.count() < 5000);
}

NNH_TEST(the_output_kept_is_bounded) {
    const std::string long_text(5000, 'x');
    const RunResult r = run_program(NNH_TOOL_TEST_CMAKE, {"-E", "echo", long_text}, 10000, 100);
    CHECK(r.started && r.exit_code == 0 && r.output.size() == 100);
}

NNH_TEST(a_missing_program_does_not_start) {
    const RunResult r = run_program("nnh-no-such-program-0f3a", {"--version"}, 2000, 4096);
    CHECK(!r.started && r.exit_code == -1 && !r.timed_out);
    const RunResult p = run_program(std::string(NNH_TOOL_TEST_TMP) + "/missing/cmake", {}, 2000, 4096);
    CHECK(!p.started);
}

NNH_TEST(windows_command_lines_quote_like_the_c_runtime) {
    CHECK(windows_command_line("C:/x/cmake.exe", {"--version"}) == "\"C:/x/cmake.exe\" --version");
    CHECK(windows_command_line("cmake", {"a b", ""}) == "\"cmake\" \"a b\" \"\"");
    CHECK(windows_command_line("cmake", {"a\"b"}) == "\"cmake\" \"a\\\"b\"");
    CHECK(windows_command_line("cmake", {"C:\\dir\\", "x\\\\y"}) == "\"cmake\" C:\\dir\\ x\\\\y");
    CHECK(windows_command_line("cmake", {"C:\\my dir\\"}) == "\"cmake\" \"C:\\my dir\\\\\"");
    CHECK(windows_command_line("cmake", {"a\\\"b c"}) == "\"cmake\" \"a\\\\\\\"b c\"");
}

NNH_TEST(the_real_probes_find_and_run_cmake) { // the one real-machine case of nnh doctor's probes (T5.4)
    const Probes p = system_probes();
    const std::string cmake = p.find_program("cmake");
    CHECK(!cmake.empty() && p.is_file(cmake) && !p.is_dir(cmake));
    const RunResult r = p.run(cmake, {"--version"});
    CHECK((r.started && r.exit_code == 0 && cmake_version(r.output) >= Version{3, 25, 0}));
    CHECK(p.free_bytes(NNH_TOOL_TEST_TMP) > 0 && !p.os_name().empty());
    CHECK(!p.list_dir(NNH_TOOL_SOURCE_DIR).empty() && p.env("NNH_DOCTOR_SURELY_UNSET_9F").empty());
}
