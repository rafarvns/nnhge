// nnh run (spec 010 FR-3..FR-5; spec 013): build, find the console, send the .3dsx to the Homebrew Launcher,
// then follow the game: its log, and a test script (--script) or the commands typed in the terminal. --pc
// runs the simulator instead (run_pc.cpp). With --script: exit 0 pass, 1 fail, 2 could not run.

#include <cstdio>
#include <optional>
#include <vector>

#include "commands.hpp"
#include "console_link.hpp"
#include "game_log.hpp"
#include "link_types.hpp"
#include "log_listener.hpp"
#include "netloader.hpp"
#include "remote/script_runner.hpp"
#include "remote/stdin_lines.hpp"
#include "remote/tunes_file.hpp"
#include "repo.hpp"
#include "user_config.hpp"

namespace nnh::tool {

namespace {

/// Reads at most `limit` bytes (more than that is the parser's "too big" error, not memory).
bool read_bytes(const std::string& path, std::vector<std::uint8_t>& out, std::size_t limit = SIZE_MAX) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::uint8_t buffer[64 * 1024];
    std::size_t n = 0;
    while (out.size() < limit && (n = std::fread(buffer, 1, sizeof buffer, f)) > 0) {
        out.insert(out.end(), buffer, buffer + n);
    }
    std::fclose(f);
    return true;
}

/// Parses --script before anything is built (spec 013 FR-3): every error, `file:line: what`.
std::optional<remote::Script> load_script(const std::string& path) {
    std::vector<std::uint8_t> bytes;
    if (!read_bytes(path, bytes, remote::kMaxScriptBytes + 1)) {
        std::fprintf(stderr, "nnh: can't read the script %s\n", path.c_str());
        return std::nullopt;
    }
    remote::ParsedScript parsed =
        remote::parse_script(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), path);
    for (const std::string& e : parsed.errors) std::fprintf(stderr, "%s\n", e.c_str());
    for (const std::string& w : parsed.script.warnings) std::printf("%s\n", w.c_str());
    if (!parsed.errors.empty()) return std::nullopt;
    return std::move(parsed.script);
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

int run_on_console(const Options& options, const GameRun& run, bool scripted) {
    const int cannot_run = scripted ? 2 : 1;
    BuiltGame built;
    if (!build_target(options, "_3dsx", options.release, built)) return cannot_run;
    std::vector<std::uint8_t> file;
    const std::string path = built.game.binary_dir + "/" + options.game + ".3dsx";
    if (!read_bytes(path, file)) {
        std::fprintf(stderr, "nnh: can't read %s\n", path.c_str());
        return cannot_run;
    }
    Settings settings = load_user_settings();
    const auto console = choose_console(options, settings);
    if (!console) {
        std::fprintf(stderr, "nnh: no console found: open the Homebrew Launcher, press Y, or pass --console <ip>\n");
        return cannot_run;
    }
    if (!is_local_network(*console)) {
        std::fprintf(stderr, "nnh: %s is not on the local network; nnh only talks to local addresses\n",
                     to_string(*console).c_str());
        return cannot_run;
    }
    LogListener logs; // listening before sending, so the first lines aren't lost
    const bool listening = logs.open(Ipv4{}, kNetloaderPort);
    std::printf("nnh: sending %s (%zu KB) to %s\n", (options.game + ".3dsx").c_str(), file.size() / 1024,
                to_string(*console).c_str());
    std::vector<std::string> extra; // spec 030: --tunes/--hold start the game held (core's held start, D52)
    if (options.hold) extra.push_back("--nnh-hold");
    const std::string error = send_3dsx(*console, kNetloaderPort, options.game + ".3dsx", file, extra);
    if (!error.empty()) {
        std::fprintf(stderr, "nnh: %s\n", error.c_str());
        return cannot_run;
    }
    settings["console"] = to_string(*console);
    save_user_settings(settings);
    if (!listening || !logs.wait_for_game(10000)) {
        std::printf("nnh: %s started; no log (Release builds don't send one%s)\n", options.game.c_str(),
                    listening ? "" : ", and port 17491 is busy here");
        return scripted ? 2 : 0;
    }
    net::Socket game = logs.take_game();
    return follow_game(game, built.root, options.game, run); // on the console the game keeps running
}

} // namespace

int command_run(const Options& options) {
    const bool scripted = !options.script.empty();
    if (!options.pc && !options.console.empty()) { // a wrong --console fails before the build
        const auto given = parse_ipv4(options.console);
        if (!given || !is_local_network(*given)) {
            std::fprintf(stderr, "nnh: --console %s is not an IPv4 address on the local network\n", options.console.c_str());
            return scripted ? 2 : 1;
        }
    }
    std::optional<remote::Script> script;
    if (scripted && !(script = load_script(options.script))) return 2;
    std::vector<remote::Step> tunes; // spec 030 FR-15: read and checked before anything is built or sent
    if (!options.tunes.empty()) {
        std::vector<std::uint8_t> bytes;
        if (!read_bytes(options.tunes, bytes, remote::kMaxTunesBytes + 1)) {
            std::fprintf(stderr, "nnh: can't read the tunes file %s\n", options.tunes.c_str());
            return 2;
        }
        const remote::TunesFile file = remote::read_tunes(
            std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), options.tunes);
        for (const std::string& e : file.errors) std::fprintf(stderr, "%s\n", e.c_str());
        if (!file.errors.empty()) return 2;
        tunes = remote::tunes_steps(file, options.tunes);
        if (scripted) script->steps.insert(script->steps.begin(), tunes.begin(), tunes.end());
    }
    remote::RunnerOptions runner_options{.interactive = !scripted, .quit_at_end = closes_game_at_end(options)};
    remote::ScriptRunner runner(scripted ? std::move(*script) : remote::Script{}, runner_options);
    StdinLines typed;
    if (!scripted) typed.start(stdin); // interactive mode: every typed line is a command (spec 013 FR-5)
    if (!scripted) runner.on_typed_steps(tunes); // origin kept: undeclared names are noted and skipped
    const GameRun run{.runner = &runner, .typed = scripted ? nullptr : &typed, .linger_ms = 1000};
    return options.pc ? run_on_pc(options, run) : run_on_console(options, run, scripted);
}

} // namespace nnh::tool
