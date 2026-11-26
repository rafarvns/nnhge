// Reconverting assets (see asset_build.hpp).

#include "reload/asset_build.hpp"

#include <chrono>
#include <thread>

namespace nnh::tool::reload {

BuildCommands build_commands(const std::string& root, const std::string& preset, const std::string& game,
                             const Shell& shell) {
    BuildCommands c;
    c.assets = cmake_command(root, "--build --preset " + preset + " --target " + game + "_assets", shell);
    c.dry_run = cmake_command(root, "--build --preset " + preset + " --target " + game + " -- -n", shell);
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

std::string first_dry_run_command(const std::vector<std::string>& lines) {
    for (const std::string& line : lines) {
        if (line.size() < 4 || line[0] != '[') continue;
        const std::size_t close = line.find("] ");
        const std::size_t slash = line.find('/');
        if (close == std::string::npos || slash == std::string::npos || slash > close) continue;
        bool numbers = true;
        for (std::size_t i = 1; i < close; ++i) numbers = numbers && (i == slash || (line[i] >= '0' && line[i] <= '9'));
        if (!numbers) continue;
        const std::string what = line.substr(close + 2);
        // CMake's CONFIGURE_DEPENDS check runs in every build, and a dry run can't know it would find nothing, so it
        // also lists CMake's re-run: neither is a command of the game (the asset build already re-ran CMake when a
        // declaration changed; what that changed in the game shows as compiles and links). Plan risk 3.
        if (what.rfind("Re-checking globbed directories", 0) == 0 || what.rfind("Re-running CMake", 0) == 0) continue;
        return what;
    }
    return "";
}

} // namespace nnh::tool::reload
