#include "serve/pairs_file.hpp"

#include <cstdio>
#include <filesystem>

namespace nnh::tool {

namespace fs = std::filesystem;

PairList parse_pairs(const std::string& text) {
    PairList out;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t eq = line.find('=');
        Id id;
        Digest secret;
        if (eq == std::string::npos || !from_hex(line.substr(0, eq), id.data(), id.size()) ||
            !from_hex(line.substr(eq + 1), secret.data(), secret.size())) {
            continue;
        }
        bool replaced = false;
        for (auto& p : out) {
            if (p.first == id) {
                p.second = secret;
                replaced = true;
            }
        }
        if (!replaced) out.emplace_back(id, secret);
    }
    return out;
}

std::string format_pairs(const PairList& pairs) {
    std::string out = "# Consoles paired with nnh serve (spec 011). Secrets: keep this file private.\n";
    for (const auto& [id, secret] : pairs) {
        out += to_hex(id.data(), id.size()) + "=" + to_hex(secret.data(), secret.size()) + "\n";
    }
    return out;
}

std::string pairs_path(const std::string& settings_file) {
    if (settings_file.empty()) return {};
    return (fs::path(settings_file).parent_path() / "nnh-pairs.ini").generic_string();
}

PairList load_pairs(const std::string& path) {
    std::FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "rb");
    if (f == nullptr) return {};
    std::string text;
    char buffer[1024];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) text.append(buffer, n);
    std::fclose(f);
    return parse_pairs(text);
}

bool save_pairs(const std::string& path, const PairList& pairs) {
    if (path.empty()) return false;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    const std::string tmp = path + ".tmp";
    std::FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr) return false;
    // Owner-only before the secrets go in (no effect on Windows, where %APPDATA% is per user).
    fs::permissions(tmp, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
    const std::string text = format_pairs(pairs);
    const bool written = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    if (std::fclose(f) != 0 || !written) return false;
    fs::rename(tmp, path, ec); // replaces the old file in one step
    return !ec;
}

} // namespace nnh::tool
