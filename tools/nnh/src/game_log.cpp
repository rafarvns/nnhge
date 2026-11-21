#include "game_log.hpp"

#include <cstdio>
#include <ctime>
#include <filesystem>

#include "remote/game_session.hpp"
#include "remote/script_runner.hpp"
#include "repo.hpp"
#include "terminal.hpp"

namespace nnh::tool {

int follow_game(net::Socket& game, const std::string& root, const std::string& name, const GameRun& run) {
    game.set_no_delay(); // remote messages are due on a frame: never held back
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    const std::string log_path = log_file_path(root, name, local);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(log_path).parent_path(), ec);
    SessionConfig config;
    config.colour = enable_colour();
    config.log = std::fopen(log_path.c_str(), "wb");
    config.shots_dir = screenshot_dir(log_path);
    config.runner = run.runner;
    config.typed = run.typed;
    config.linger_ms = run.linger_ms;
    std::printf("nnh: %s is running; its log follows (also in %s)\n", name.c_str(), log_path.c_str());
    std::fflush(stdout);
    const int code = run_game_session(game, config);
    if (config.log != nullptr) std::fclose(config.log);
    const bool scripted = run.runner != nullptr && !run.runner->interactive();
    if (!scripted) std::printf("nnh: %s closed\n", name.c_str()); // a script ends with its verdict instead
    return code;
}

bool show_game_log(LogListener& logs, const std::string& root, const std::string& game, int wait_ms) {
    if (!logs.wait_for_game(wait_ms)) return false;
    net::Socket connection = logs.take_game();
    follow_game(connection, root, game, GameRun{});
    return true;
}

} // namespace nnh::tool
