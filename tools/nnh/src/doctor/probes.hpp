#pragma once
// What nnh doctor may look at (spec 033 FR-18, FR-23, D56 item 5): the environment, the file system (read
// only), programs run with a time limit (process_run.hpp), free space, the system's name, and, only when
// asked for, the network. Every check reads the machine through this seam: the real probes are in
// probes_system.cpp, the tests build fakes field by field (tools/nnh/tests/doctor_fakes.hpp), so every rule
// is tested without the tools installed.

#include "process_run.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace nnh::tool {

inline constexpr int kDoctorProgramTimeoutMs = 10000; ///< each program nnh doctor runs (doctor_limits.hpp)
inline constexpr std::size_t kDoctorMaxOutput = 64u * 1024u; ///< bytes of each program's output kept

struct Probes {
    std::function<std::string(const char* name)> env; ///< "" when unset
    std::function<bool(const std::string& path)> is_file;
    std::function<bool(const std::string& path)> is_dir;
    std::function<std::string(const std::string& path, std::size_t limit)> read_file; ///< "" when unreadable
    std::function<std::vector<std::string>(const std::string& dir)> list_dir;         ///< entry names, sorted
    std::function<std::string(const char* name)> find_program; ///< a full path, or "" when not on PATH
    /// Runs a program (no shell) with the doctor's time limit and output bound.
    std::function<RunResult(const std::string& program, const std::vector<std::string>& args)> run;
    std::function<std::int64_t(const std::string& path)> free_bytes; ///< -1 when unknown
    std::function<std::string()> os_name; ///< "Windows 11 (10.0.26100)", "Ubuntu 24.04"
    // The network: called only with --console / --online (AC-8).
    /// Finds a console the way nnh run does (the netloader's answer); "" when none answered.
    std::function<std::string(const std::string& console)> find_console;
    /// Fetches one byte of `url` with `cmake` (`cainfo`: a CA bundle, "" = CMake's own), from a temporary
    /// script that is removed afterwards; "" when it worked, else CMake's status text.
    std::function<std::string(const std::string& cmake, const std::string& url, const std::string& cainfo)>
        fetch;
};

/// The real probes of this machine.
Probes system_probes();

} // namespace nnh::tool
