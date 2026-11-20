#include "game_log.hpp"

#include <cstdio>
#include <ctime>
#include <filesystem>

#include "log_view.hpp"
#include "repo.hpp"
#include "terminal.hpp"

namespace nnh::tool {

namespace {

struct LogSink {
    std::FILE* file = nullptr;
    bool colour = false;
};

void show_line(const std::string& line, void* context) {
    auto* sink = static_cast<LogSink*>(context);
    std::printf("%s\n", render_log_line(line, sink->colour).c_str());
    std::fflush(stdout);
    if (sink->file != nullptr) {
        std::fprintf(sink->file, "%s\n", line.c_str());
        std::fflush(sink->file);
    }
}

} // namespace

bool show_game_log(LogListener& logs, const std::string& root, const std::string& game, int wait_ms) {
    if (!logs.wait_for_game(wait_ms)) return false;
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    const std::string log_path = log_file_path(root, game, local);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(log_path).parent_path(), ec);
    LogSink sink{std::fopen(log_path.c_str(), "wb"), enable_colour()};
    std::printf("nnh: %s is running; its log follows (also in %s)\n", game.c_str(), log_path.c_str());
    logs.read_lines(show_line, &sink);
    if (sink.file != nullptr) std::fclose(sink.file);
    std::printf("nnh: %s closed\n", game.c_str());
    return true;
}

} // namespace nnh::tool
