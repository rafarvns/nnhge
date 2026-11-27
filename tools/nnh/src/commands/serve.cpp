// nnh serve (spec 011 FR-1): wait for NNH Link on the console — answer its discovery, pair or authenticate
// it, list and build games for it, send them, and show the log of the game it starts. One console at a time,
// local network only, until Ctrl+C. nnh unpair forgets every paired console.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <random>
#include <thread>

#include "build_info.hpp"
#include "commands.hpp"
#include "game_log.hpp"
#include "link_types.hpp"
#include "log_listener.hpp"
#include "netloader.hpp"
#include "nnh_link/server_session.hpp"
#include "protocol_mismatch.hpp"
#include "repo.hpp"
#include "serve/discovery_responder.hpp"
#include "serve/serve_hooks.hpp"
#include "user_config.hpp"

namespace nnh::tool {

namespace {

std::uint32_t random_u32() {
    static std::random_device device;
    return device();
}

template <std::size_t N>
std::array<std::uint8_t, N> random_bytes() {
    std::array<std::uint8_t, N> out;
    for (std::size_t i = 0; i < N; i += 4) {
        const std::uint32_t r = random_u32();
        for (std::size_t k = 0; k < 4 && i + k < N; ++k) out[i + k] = static_cast<std::uint8_t>(r >> (8 * k));
    }
    return out;
}

std::int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

/// The server id from nnh.ini, made (and saved) on first use.
Id server_id() {
    Settings settings = load_user_settings();
    Id id;
    const auto saved = settings.find("server_id");
    if (saved != settings.end() && from_hex(saved->second, id.data(), id.size())) return id;
    id = random_bytes<16>();
    settings["server_id"] = to_hex(id.data(), id.size());
    save_user_settings(settings);
    return id;
}

} // namespace

int command_serve(const Options& options) {
    // Messages (the pairing code) must show at once even through a pipe or a file (Windows has no line
    // buffering: _IOLBF is full buffering there).
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const std::string root = open_project(); // a game project or the engine repository (spec 032)
    if (root.empty()) return 1;
    net::Socket listener = net::tcp_listen(Ipv4{}, link::kLinkPort);
    net::Socket udp = net::udp_open(Ipv4{}, link::kLinkPort);
    if (!listener.valid() || !udp.valid()) {
        std::fprintf(stderr, "nnh: port %u is busy: is another nnh serve running?\n", link::kLinkPort);
        return 1;
    }
    LogListener logs;
    const bool logs_ok = logs.open(Ipv4{}, kNetloaderPort);
    const Id id = server_id();
    std::string name = net::host_name();
    if (name.empty()) name = "nnh";

    ServeHooks hooks(root, options.release, pairs_path(user_settings_path()), now_ms, random_u32);
    std::atomic<bool> running{true};
    std::thread discovery([&] {
        while (running) answer_discovery(udp, id, name, 500);
    });
    std::printf("nnh: serving %s as \"%s\" on port %u (%s builds); Ctrl+C stops\n", root.c_str(), name.c_str(),
                link::kLinkPort, options.release ? "Release" : "Development");
    if (!logs_ok) std::printf("nnh: port %u is busy: games' logs won't show here\n", kNetloaderPort);
    hooks.show_new_code();

    link::PairingGuard guard;
    for (;;) {
        Ipv4 peer;
        net::Socket console = net::tcp_accept(listener, 1000, &peer);
        if (!console.valid()) continue;
        if (!is_local_network(peer)) {
            std::printf("nnh: refused %s (not on the local network)\n", to_string(peer).c_str());
            continue;
        }
        link::Hello mine;
        mine.id = id;
        mine.nonce = random_bytes<16>();
        const std::string end = link::serve_console(static_cast<net::Socket&&>(console), mine, hooks, guard);
        std::printf("nnh: %s\n", serve_end_text(end, build_info().version).c_str()); // spec 033 FR-22
        const std::string game = hooks.take_launch();
        if (!game.empty() && logs_ok && !show_game_log(logs, root, game, 20000)) {
            std::printf("nnh: %s started; no log (Release builds don't send one)\n", game.c_str());
        }
    }
}

int command_unpair() {
    const std::string path = pairs_path(user_settings_path());
    const PairList pairs = load_pairs(path);
    if (pairs.empty()) {
        std::printf("nnh: no console is paired\n");
        return 0;
    }
    if (!save_pairs(path, {})) {
        std::fprintf(stderr, "nnh: can't write %s\n", path.c_str());
        return 1;
    }
    std::printf("nnh: forgot %zu console(s); they must pair again with nnh serve\n", pairs.size());
    return 0;
}

} // namespace nnh::tool
