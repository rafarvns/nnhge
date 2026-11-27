// Reconverting assets (see asset_build.hpp).

#include "reload/asset_build.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

namespace nnh::tool::reload {

std::string cached_ninja(const std::string& build_dir) {
    std::FILE* f = std::fopen((build_dir + "/CMakeCache.txt").c_str(), "rb");
    if (f == nullptr) return "";
    char line[1024];
    std::string found;
    const std::string key = "CMAKE_MAKE_PROGRAM:FILEPATH=";
    while (std::fgets(line, sizeof line, f) != nullptr) {
        std::string s(line);
        if (s.rfind(key, 0) != 0) continue;
        found = s.substr(key.size());
        while (!found.empty() && (found.back() == '\n' || found.back() == '\r')) found.pop_back();
        break;
    }
    std::fclose(f);
    const std::size_t slash = found.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? found : found.substr(slash + 1);
    return name == "ninja" || name == "ninja.exe" ? found : "";
}

BuildCommands build_commands(const std::string& root, const std::string& preset, const std::string& game,
                             const Shell& shell, const std::string& ninja) {
    BuildCommands c;
    if (!ninja.empty() && shell.devkitpro_bash.empty()) {
        const std::string base = "\"" + ninja + "\" -C \"" + root + "/build/" + preset + "\" ";
        c.assets = base + game + "_assets";
        c.program_inputs = base + "-t inputs " + game;
        return c;
    }
    if (!ninja.empty()) { // the 3DS on Windows: devkitPro's Ninja inside its shell (T10.3: no `cmake --build` start-up)
        const std::string base = "\"" + shell.devkitpro_bash + "\" -lc \"cd '" + msys_path(root) + "' && " + ninja +
                                 " -C build/" + preset + " ";
        c.assets = base + game + "_assets\"";
        c.program_inputs = base + "-t inputs " + game + "\"";
        return c;
    }
    c.assets = cmake_command(root, "--build --preset " + preset + " --target " + game + "_assets", shell);
    return c;
}

CommandRunner shell_runner(const Shell& shell) {
    return [shell](const std::string& command, std::vector<std::string>& lines) {
        return run_shell_lines(command, shell, [&lines](const std::string& line) {
            lines.push_back(line);
            return true;
        });
    };
}

BuildResult build_assets(const BuildCommands& commands, const CommandRunner& run, int retry_ms) {
    BuildResult r;
    for (int attempt = 0; attempt < 2 && !r.ok; ++attempt) {
        if (attempt > 0) std::this_thread::sleep_for(std::chrono::milliseconds(retry_ms));
        r.lines.clear();
        r.ok = run(commands.assets, r.lines) == 0;
        r.attempts = attempt + 1;
    }
    return r;
}

namespace {

std::string comparable(std::string path) {
    path = native_path(path); // "/e/x" -> "E:/x"
    for (char& c : path) {
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return path;
}

bool absolute(const std::string& path) {
    return !path.empty() && (path[0] == '/' || path[0] == '\\' || (path.size() > 1 && path[1] == ':'));
}

} // namespace

std::string program_input(const std::vector<std::string>& inputs, const std::string& source,
                          const std::string& build_dir) {
    const std::string want = comparable(source);
    for (const std::string& line : inputs) {
        const std::string full = absolute(line) ? line : build_dir + "/" + line;
        if (comparable(full) == want) return line;
    }
    return "";
}

} // namespace nnh::tool::reload
