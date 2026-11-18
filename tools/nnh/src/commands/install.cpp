// nnh install (spec 010 FR-6): build the game's .cia, serve it on this computer's address towards the
// console, and hand its URL to FBI's "Receive URLs over the network".

#include <cstdio>
#include <filesystem>

#include "commands.hpp"
#include "console_link.hpp"
#include "fbi.hpp"
#include "file_server.hpp"
#include "net.hpp"
#include "user_config.hpp"

namespace nnh::tool {

int command_install(const Options& options) {
    Settings settings = load_user_settings();
    std::string address = options.console;
    if (address.empty()) {
        const auto remembered = settings.find("console");
        if (remembered != settings.end()) address = remembered->second;
    }
    const auto console = parse_ipv4(address);
    if (!console) {
        std::fprintf(stderr, "nnh: which console? pass --console <ip> (FBI shows it on 'Receive URLs over the network')\n");
        return 1;
    }
    if (!is_local_network(*console)) {
        std::fprintf(stderr, "nnh: %s is not on the local network; nnh only talks to local addresses\n", address.c_str());
        return 1;
    }
    BuiltGame built;
    if (!build_target(options, "_cia", true, built, true)) return 1;
    const std::string name = options.game + ".cia";
    const std::string path = built.root + "/build/" + built.preset + "/dist/" + options.game + "/" + name;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        std::fprintf(stderr, "nnh: %s wasn't built\n", path.c_str());
        return 1;
    }
    const auto local = net::local_address_towards(*console);
    FileServer server;
    if (!local || !server.start(*local, 0, {{"/" + name, path}})) {
        std::fprintf(stderr, "nnh: can't serve the file to %s\n", address.c_str());
        return 1;
    }
    const std::string url = http_url(to_string(*local), server.port(), name);
    std::printf("nnh: serving %s; sending its URL to FBI at %s (accept the install on the console)\n", url.c_str(),
                address.c_str());
    const std::string error = fbi_send_urls(*console, kFbiPort, {url}, 10 * 60 * 1000);
    const int sent = server.served();
    server.stop();
    if (!error.empty()) {
        std::fprintf(stderr, "nnh: %s\n", error.c_str());
        return 1;
    }
    settings["console"] = address;
    save_user_settings(settings);
    std::printf("nnh: FBI is done (%s downloaded %d time%s)\n", name.c_str(), sent, sent == 1 ? "" : "s");
    return sent > 0 ? 0 : 1;
}

} // namespace nnh::tool
