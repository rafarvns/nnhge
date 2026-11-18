// nnh run (spec 010 FR-3..FR-5): build, find the console, send the .3dsx to the Homebrew Launcher, then show
// and save the game's log until it exits. --pc runs the simulator instead.

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <vector>

#include "commands.hpp"
#include "console_link.hpp"
#include "log_listener.hpp"
#include "log_view.hpp"
#include "net.hpp"
#include "netloader.hpp"
#include "repo.hpp"
#include "terminal.hpp"
#include "user_config.hpp"

namespace nnh::tool {

namespace {

bool read_bytes(const std::string& path, std::vector<std::uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::uint8_t buffer[64 * 1024];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) out.insert(out.end(), buffer, buffer + n);
    std::fclose(f);
    return true;
}

/// The console to use: --console, else whichever answers the discovery, else the remembered one.
std::optional<Ipv4> choose_console(const Options& options, const Settings& settings) {
    if (!options.console.empty()) return parse_ipv4(options.console);
    std::printf("nnh: looking for the console (Homebrew Launcher open, Y pressed)...\n");
    // Ask the remembered console directly, every local network's broadcast address (255.255.255.255
    // leaves through one interface only, often a VPN's), and the global broadcast.
    std::vector<Ipv4> targets;
    const auto remembered = settings.find("console");
    const auto last = remembered != settings.end() ? parse_ipv4(remembered->second) : std::nullopt;
    if (last) targets.push_back(*last);
    for (const Ipv4& b : net::local_broadcast_addresses()) targets.push_back(b);
    targets.push_back(Ipv4{{255, 255, 255, 255}});
    if (auto found = discover_console(targets, kNetloaderPort, kNetloaderPort, 3, 1000)) {
        std::printf("nnh: found the console at %s\n", to_string(*found).c_str());
        return found;
    }
    if (last) std::printf("nnh: no answer; trying the last console used, %s\n", to_string(*last).c_str());
    return last;
}

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

int run_on_pc(const Options& options) {
    BuiltGame built;
    if (!build_target(options, "", options.release, built)) return 1;
#ifdef _WIN32
    const std::string exe = built.game.binary_dir + "/" + options.game + ".exe";
#else
    const std::string exe = built.game.binary_dir + "/" + options.game;
#endif
    const Shell shell = host_shell(false);
    return run_shell("\"" + exe + "\"", shell);
}

} // namespace

int command_run(const Options& options) {
    if (options.pc) return run_on_pc(options);
    if (!options.console.empty()) { // a wrong --console fails before the build
        const auto given = parse_ipv4(options.console);
        if (!given || !is_local_network(*given)) {
            std::fprintf(stderr, "nnh: --console %s is not an IPv4 address on the local network\n", options.console.c_str());
            return 1;
        }
    }
    BuiltGame built;
    if (!build_target(options, "_3dsx", options.release, built)) return 1;
    std::vector<std::uint8_t> file;
    const std::string path = built.game.binary_dir + "/" + options.game + ".3dsx";
    if (!read_bytes(path, file)) {
        std::fprintf(stderr, "nnh: can't read %s\n", path.c_str());
        return 1;
    }
    Settings settings = load_user_settings();
    const auto console = choose_console(options, settings);
    if (!console) {
        std::fprintf(stderr, "nnh: no console found: open the Homebrew Launcher, press Y, or pass --console <ip>\n");
        return 1;
    }
    if (!is_local_network(*console)) {
        std::fprintf(stderr, "nnh: %s is not on the local network; nnh only talks to local addresses\n",
                     to_string(*console).c_str());
        return 1;
    }
    LogListener logs; // listening before sending, so the first lines aren't lost
    const bool listening = logs.open(Ipv4{}, kNetloaderPort);
    std::printf("nnh: sending %s (%zu KB) to %s\n", (options.game + ".3dsx").c_str(), file.size() / 1024,
                to_string(*console).c_str());
    const std::string error = send_3dsx(*console, kNetloaderPort, options.game + ".3dsx", file, {});
    if (!error.empty()) {
        std::fprintf(stderr, "nnh: %s\n", error.c_str());
        return 1;
    }
    settings["console"] = to_string(*console);
    save_user_settings(settings);
    if (!listening || !logs.wait_for_game(10000)) {
        std::printf("nnh: %s started; no log (Release builds don't send one%s)\n", options.game.c_str(),
                    listening ? "" : ", and port 17491 is busy here");
        return 0;
    }
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    const std::string log_path = log_file_path(built.root, options.game, local);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(log_path).parent_path(), ec);
    LogSink sink{std::fopen(log_path.c_str(), "wb"), enable_colour()};
    std::printf("nnh: %s is running; its log follows (also in %s)\n", options.game.c_str(), log_path.c_str());
    logs.read_lines(show_line, &sink);
    if (sink.file != nullptr) std::fclose(sink.file);
    std::printf("nnh: %s closed\n", options.game.c_str());
    return 0;
}

} // namespace nnh::tool
