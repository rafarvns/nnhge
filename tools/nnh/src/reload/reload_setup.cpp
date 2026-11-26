// Where a game's assets are (see reload_setup.hpp).

#include "reload/reload_setup.hpp"

#include <cstdio>

#include "repo.hpp"

namespace nnh::tool::reload {

std::optional<ReloadPaths> parse_reload_paths(const std::string& text) {
    ReloadPaths p;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t eq = line.find('=');
        if (eq != std::string::npos) {
            const std::string key = line.substr(0, eq), value = native_path(line.substr(eq + 1));
            if (key == "assets") p.assets = value;
            else if (key == "game") p.game = value;
            else if (key == "out") p.out = value;
        }
        at = end + 1;
    }
    if (p.assets.empty() || p.game.empty() || p.out.empty()) return std::nullopt;
    return p;
}

std::optional<ReloadPaths> read_reload_paths(const std::string& binary_dir, const std::string& game) {
    std::FILE* f = std::fopen((binary_dir + "/nnh_reload_" + game + ".txt").c_str(), "rb");
    if (f == nullptr) return std::nullopt;
    std::string text;
    char buffer[1024];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) text.append(buffer, n);
    std::fclose(f);
    return parse_reload_paths(text);
}

} // namespace nnh::tool::reload
