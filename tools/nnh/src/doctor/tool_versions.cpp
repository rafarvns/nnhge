// Reading tool versions from what the tools print (tool_versions.hpp).

#include "doctor/tool_versions.hpp"

namespace nnh::tool {
namespace {

constexpr std::size_t kMaxRead = 64u * 1024u;
constexpr int kMaxDigits = 6;

bool digit(char c) { return c >= '0' && c <= '9'; }

/// The number at `i` (at most kMaxDigits digits), advancing `i`; -1 when there is none or it is too long.
int number_at(std::string_view t, std::size_t& i) {
    int value = 0, digits = 0;
    while (i < t.size() && digit(t[i])) {
        if (++digits > kMaxDigits) return -1;
        value = value * 10 + (t[i++] - '0');
    }
    return digits == 0 ? -1 : value;
}

/// The version that starts exactly at `i` ("n.n" or "n.n.n"), or invalid.
Version version_at(std::string_view t, std::size_t i) {
    Version v;
    const int major = number_at(t, i);
    if (major < 0 || i >= t.size() || t[i] != '.') return {};
    ++i;
    const int minor = number_at(t, i);
    if (minor < 0) return {};
    int patch = 0;
    if (i + 1 < t.size() && t[i] == '.' && digit(t[i + 1])) {
        ++i;
        patch = number_at(t, i);
        if (patch < 0) return {};
    }
    if (i < t.size() && digit(t[i])) return {};
    v.major = major;
    v.minor = minor;
    v.patch = patch;
    return v;
}

std::string_view first_line(std::string_view t) {
    t = t.substr(0, std::min(t.size(), kMaxRead));
    const std::size_t end = t.find_first_of("\r\n");
    return end == std::string_view::npos ? t : t.substr(0, end);
}

bool contains(std::string_view t, std::string_view part) { return t.find(part) != std::string_view::npos; }

} // namespace

std::string Version::text() const {
    if (!valid()) return "";
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

Version first_version(std::string_view text, std::size_t from) {
    text = text.substr(0, std::min(text.size(), kMaxRead));
    for (std::size_t i = from; i < text.size(); ++i) {
        if (!digit(text[i]) || (i > 0 && (digit(text[i - 1]) || text[i - 1] == '.'))) continue; // a number's start
        const Version v = version_at(text, i);
        if (v.valid()) return v;
    }
    return {};
}

Version cmake_version(std::string_view output) {
    const std::string_view line = first_line(output);
    static constexpr std::string_view kPrefix = "cmake version ";
    if (line.substr(0, kPrefix.size()) != kPrefix) return {};
    return version_at(line, kPrefix.size());
}

Version ninja_version(std::string_view output) { return version_at(first_line(output), 0); }

CompilerVersion compiler_version(std::string_view output) {
    const std::string_view line = first_line(output);
    CompilerVersion c;
    if (const std::size_t at = line.find("Optimizing Compiler Version "); at != std::string_view::npos) {
        c.kind = CompilerKind::Msvc;
        c.version = first_version(line, at);
    } else if (const std::size_t apple = line.find("Apple clang version "); apple != std::string_view::npos) {
        c.kind = CompilerKind::AppleClang;
        c.version = first_version(line, apple);
    } else if (const std::size_t clang = line.find("clang version "); clang != std::string_view::npos) {
        c.kind = CompilerKind::Clang;
        c.version = first_version(line, clang);
    } else if (contains(line, "(GCC)") || contains(line, "g++") || contains(line, "gcc") || contains(line, "c++")) {
        // "g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0": the version follows the last ')'
        const std::size_t close = line.rfind(')');
        c.kind = CompilerKind::Gcc;
        c.version = first_version(line, close == std::string_view::npos ? 0 : close);
    }
    if (!c.version.valid()) c.kind = CompilerKind::Unknown;
    return c;
}

std::string pacman_version(const std::vector<std::string>& folder_names, std::string_view name) {
    for (const std::string& folder : folder_names) {
        // "<name>-<version>-<release>": the name itself may hold '-', the version and release don't
        const std::size_t release = folder.rfind('-');
        if (release == std::string::npos || release == 0) continue;
        const std::size_t version = folder.rfind('-', release - 1);
        if (version == std::string::npos || std::string_view(folder).substr(0, version) != name) continue;
        return folder.substr(version + 1, release - version - 1);
    }
    return "";
}

} // namespace nnh::tool
