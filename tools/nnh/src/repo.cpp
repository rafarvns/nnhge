#include "repo.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace nnh::tool {

namespace fs = std::filesystem;

std::string find_repo_root(const std::string& start) {
    std::error_code ec;
    fs::path at = fs::absolute(start, ec);
    for (int depth = 0; depth < 64 && !at.empty(); ++depth) {
        if (fs::exists(at / "CMakePresets.json", ec) && fs::is_directory(at / "engine" / "core", ec)) {
            return at.generic_string();
        }
        const fs::path parent = at.parent_path();
        if (parent == at) break;
        at = parent;
    }
    return {};
}

std::string preset_name(bool pc, bool release) {
    return std::string(pc ? "pc-" : "3ds-") + (release ? "release" : "dev");
}

std::string native_path(const std::string& path) {
    const bool drive = path.size() >= 2 && path[0] == '/' && std::isalpha(static_cast<unsigned char>(path[1])) &&
                       (path.size() == 2 || path[2] == '/');
    if (!drive) return path;
    std::string out(1, static_cast<char>(std::toupper(static_cast<unsigned char>(path[1]))));
    out += ":/";
    if (path.size() > 3) out += path.substr(3);
    return out;
}

std::string msys_path(const std::string& path) {
    if (path.size() < 2 || path[1] != ':' || !std::isalpha(static_cast<unsigned char>(path[0]))) return path;
    std::string out = "/";
    out += static_cast<char>(std::tolower(static_cast<unsigned char>(path[0])));
    for (std::size_t i = 2; i < path.size(); ++i) out += path[i] == '\\' ? '/' : path[i];
    return out;
}

std::string cmake_command(const std::string& root, const std::string& args, const Shell& shell) {
    if (!shell.devkitpro_bash.empty()) {
        return "\"" + shell.devkitpro_bash + "\" -lc \"cd '" + msys_path(root) + "' && cmake " + args + "\"";
    }
    if (shell.windows) return "cd /d \"" + root + "\" && cmake " + args;
    return "cd '" + root + "' && cmake " + args;
}

Shell host_shell(bool for_3ds) {
    Shell shell;
#ifdef _WIN32
    shell.windows = true;
    if (for_3ds) {
        const char* msys2 = std::getenv("DEVKITPRO_MSYS2");
        shell.devkitpro_bash = std::string(msys2 != nullptr && *msys2 ? msys2 : "C:/devkitPro/msys2") + "/usr/bin/bash.exe";
    }
#else
    (void)for_3ds;
#endif
    return shell;
}

int run_shell(const std::string& command, const Shell& shell) {
    std::fflush(stdout);
    // cmd.exe drops the first and last quote of a command that starts with one: wrap it once more.
    const std::string line = shell.windows ? "\"" + command + "\"" : command;
    return std::system(line.c_str());
}

int run_shell_lines(const std::string& command, const Shell& shell,
                    const std::function<bool(const std::string&)>& on_line) {
    std::fflush(stdout);
    const std::string line = shell.windows ? "\"" + command + " 2>&1\"" : command + " 2>&1";
#ifdef _WIN32
    std::FILE* pipe = _popen(line.c_str(), "rb");
#else
    std::FILE* pipe = popen(line.c_str(), "r");
#endif
    if (pipe == nullptr) return -1;
    std::string pending;
    bool stopped = false;
    char buffer[4096];
    std::size_t n = 0;
    while (!stopped && (n = std::fread(buffer, 1, sizeof buffer, pipe)) > 0) {
        pending.append(buffer, n);
        std::size_t start = 0;
        for (std::size_t nl; !stopped && (nl = pending.find('\n', start)) != std::string::npos; start = nl + 1) {
            std::string text = pending.substr(start, nl - start);
            if (!text.empty() && text.back() == '\r') text.pop_back();
            stopped = !on_line(text);
        }
        pending.erase(0, start);
    }
    if (!stopped && !pending.empty()) {
        if (pending.back() == '\r') pending.pop_back();
        stopped = !on_line(pending);
    }
#ifdef _WIN32
    const int status = _pclose(pipe);
#else
    const int raw = pclose(pipe);
    const int status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
#endif
    return stopped ? -1 : status;
}

std::string log_file_path(const std::string& root, const std::string& game, const std::tm& t) {
    char stamp[32];
    std::snprintf(stamp, sizeof stamp, "%04d%02d%02d-%02d%02d%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour,
                  t.tm_min, t.tm_sec);
    return root + "/build/logs/" + game + "-" + stamp + ".txt";
}

} // namespace nnh::tool
