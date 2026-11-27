#pragma once
// A fake machine for nnh doctor's rule tests (doctor/probes.hpp): environment variables, files, folders, programs and
// what they print, free space, and a network that only counts how often it was used (AC-8). Build one, set what the
// case needs, then `probes()`.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "doctor/probes.hpp"

namespace nnh::tool::test {

struct FakeMachine {
    std::map<std::string, std::string> env;
    std::set<std::string> files;                      ///< existing files (generic paths)
    std::map<std::string, std::string> file_text;     ///< readable files' contents (also existing files)
    std::map<std::string, std::vector<std::string>> dirs; ///< existing folders and their entries
    std::map<std::string, std::string> programs;      ///< name on PATH -> full path
    std::map<std::string, RunResult> outputs;         ///< "<program> <args...>" -> what it printed
    std::int64_t free = 50ll * 1024 * 1024 * 1024;
    std::string os = "Windows 11 (10.0.26100)";
    std::string console_found;                        ///< what find_console answers
    std::string fetch_error;                          ///< what fetch answers ("" = reachable)

    std::shared_ptr<int> network_calls = std::make_shared<int>(0);
    std::shared_ptr<std::vector<std::string>> fetched = std::make_shared<std::vector<std::string>>();
    std::shared_ptr<std::vector<std::string>> ran = std::make_shared<std::vector<std::string>>();

    /// A program on PATH printing `output` (exit 0) for `args`.
    void program(const std::string& name, const std::string& path, const std::string& args, const std::string& output) {
        programs[name] = path;
        files.insert(path);
        outputs[path + " " + args] = RunResult{true, 0, false, output};
    }

    Probes probes() const {
        Probes p;
        const FakeMachine m = *this; // a copy: the probes outlive the test's changes
        p.env = [m](const char* name) {
            const auto it = m.env.find(name);
            return it == m.env.end() ? std::string() : it->second;
        };
        p.is_file = [m](const std::string& path) { return m.files.count(path) > 0 || m.file_text.count(path) > 0; };
        p.is_dir = [m](const std::string& path) { return m.dirs.count(path) > 0; };
        p.read_file = [m](const std::string& path, std::size_t limit) {
            const auto it = m.file_text.find(path);
            return it == m.file_text.end() ? std::string() : it->second.substr(0, limit + 1);
        };
        p.list_dir = [m](const std::string& dir) {
            const auto it = m.dirs.find(dir);
            return it == m.dirs.end() ? std::vector<std::string>() : it->second;
        };
        p.find_program = [m](const char* name) {
            const auto it = m.programs.find(name);
            return it == m.programs.end() ? std::string() : it->second;
        };
        p.run = [m](const std::string& program, const std::vector<std::string>& args) {
            std::string key = program;
            for (const std::string& a : args) key += " " + a;
            m.ran->push_back(key);
            const auto it = m.outputs.find(key);
            return it == m.outputs.end() ? RunResult{} : it->second;
        };
        p.free_bytes = [m](const std::string&) { return m.free; };
        p.os_name = [m] { return m.os; };
        p.find_console = [m](const std::string&) {
            ++*m.network_calls;
            return m.console_found;
        };
        p.fetch = [m](const std::string&, const std::string& url, const std::string&) {
            ++*m.network_calls;
            m.fetched->push_back(url);
            return m.fetch_error;
        };
        return p;
    }
};

} // namespace nnh::tool::test
