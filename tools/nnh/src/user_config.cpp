#include "user_config.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace nnh::tool {

namespace {

std::string trim(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return {};
    const std::size_t b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr ? v : "";
}

} // namespace

Settings parse_settings(const std::string& text) {
    Settings out;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        const std::string line = trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty() || line[0] == '#') continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        if (!key.empty()) out[key] = trim(line.substr(eq + 1));
    }
    return out;
}

std::string format_settings(const Settings& settings) {
    std::string out = "# nnh settings (spec 010)\n";
    for (const auto& [key, value] : settings) out += key + "=" + value + "\n";
    return out;
}

std::string settings_path(const std::string& appdata, const std::string& xdg_config_home, const std::string& home) {
    if (!appdata.empty()) return appdata + "/nnhge/nnh.ini";
    if (!xdg_config_home.empty()) return xdg_config_home + "/nnhge/nnh.ini";
    if (!home.empty()) return home + "/.config/nnhge/nnh.ini";
    return {};
}

Settings load_user_settings() {
    const std::string path = settings_path(env("APPDATA"), env("XDG_CONFIG_HOME"), env("HOME"));
    std::FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "rb");
    if (f == nullptr) return {};
    std::string text;
    char buffer[1024];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) text.append(buffer, n);
    std::fclose(f);
    return parse_settings(text);
}

bool save_user_settings(const Settings& settings) {
    const std::string path = settings_path(env("APPDATA"), env("XDG_CONFIG_HOME"), env("HOME"));
    if (path.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    const std::string text = format_settings(settings);
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    return std::fclose(f) == 0 && ok;
}

} // namespace nnh::tool
