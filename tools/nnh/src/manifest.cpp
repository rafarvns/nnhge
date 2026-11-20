#include "manifest.hpp"

namespace nnh::tool {

std::vector<GameEntry> parse_manifest(const std::string& text) {
    std::vector<GameEntry> games;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t a = line.find('|');
        const std::size_t b = a == std::string::npos ? a : line.find('|', a + 1);
        if (b == std::string::npos) continue;
        GameEntry g{line.substr(0, a), line.substr(a + 1, b - a - 1), false, ""};
        const std::size_t c = line.find('|', b + 1); // the optional title: the rest of the line
        const std::string cia = line.substr(b + 1, c == std::string::npos ? std::string::npos : c - b - 1);
        g.title = c == std::string::npos || c + 1 == line.size() ? g.name : line.substr(c + 1);
        if (g.name.empty() || g.binary_dir.empty() || (cia != "0" && cia != "1")) continue;
        g.cia = cia == "1";
        for (GameEntry& existing : games) {
            if (existing.name == g.name) {
                existing = g;
                g.name.clear();
            }
        }
        if (!g.name.empty()) games.push_back(g);
    }
    return games;
}

std::optional<GameEntry> find_game(const std::vector<GameEntry>& games, const std::string& name) {
    for (const GameEntry& g : games) {
        if (g.name == name) return g;
    }
    return std::nullopt;
}

} // namespace nnh::tool
