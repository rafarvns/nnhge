// What nnh doctor reads from a project's files (project_versions.hpp).

#include "doctor/project_versions.hpp"

namespace nnh::tool {
namespace {

bool is_version(std::string_view v) {
    int dots = 0;
    bool digit = false;
    for (char c : v) {
        if (c >= '0' && c <= '9')
            digit = true;
        else if (c == '.' && digit)
            ++dots, digit = false;
        else
            return false;
    }
    return dots == 2 && digit && v.size() <= 32;
}

/// Each line of `text`, '\r' removed, until `visit` returns true.
template <typename Visit> void each_line(std::string_view text, Visit visit) {
    for (std::size_t start = 0; start < text.size();) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (visit(line)) return;
        start = end + 1;
    }
}

} // namespace

std::string cache_engine_version(std::string_view cache) {
    std::string found;
    each_line(cache, [&](std::string_view line) {
        static constexpr std::string_view kKey = "NNHGE_VERSION:";
        if (line.substr(0, kKey.size()) != kKey) return false;
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) return false;
        const std::string_view value = line.substr(eq + 1);
        if (is_version(value)) found = std::string(value);
        return true;
    });
    return found;
}

std::string project_pin_url(std::string_view cmakelists) {
    std::string found;
    each_line(cmakelists, [&](std::string_view line) {
        static constexpr std::string_view kStart = "set(nnhge_url \"";
        if (line.substr(0, kStart.size()) != kStart) return false;
        const std::size_t end = line.find('"', kStart.size());
        if (end != std::string_view::npos)
            found = std::string(line.substr(kStart.size(), end - kStart.size()));
        return true;
    });
    return found;
}

} // namespace nnh::tool
