// The values nnh new writes into a project (project_name.hpp).

#include "new/project_name.hpp"

#include <array>
#include <cstdio>

namespace nnh::tool {
namespace {

constexpr std::size_t kMaxName = 32;
constexpr std::size_t kMaxText = 64;

const char* const kReservedTargets[] = {"dist",
                                        "all",
                                        "clean",
                                        "install",
                                        "test",
                                        "help",
                                        "package",
                                        "package_source",
                                        "edit_cache",
                                        "rebuild_cache",
                                        "list_install_components",
                                        "nnh"};

bool lower(char c) {
    return c >= 'a' && c <= 'z';
}
bool digit(char c) {
    return c >= '0' && c <= '9';
}
bool name_char(char c) {
    return lower(c) || digit(c) || c == '_' || c == '-';
}

bool windows_device(std::string_view n) {
    if (n == "con" || n == "nul" || n == "aux" || n == "prn") return true;
    return n.size() == 4 && (n.substr(0, 3) == "com" || n.substr(0, 3) == "lpt") && n[3] >= '1' &&
           n[3] <= '9';
}

bool reserved_target(std::string_view n) {
    for (const char* r : kReservedTargets) {
        if (n == r) return true;
    }
    return n.substr(0, 4) == "nnh_" || n.substr(0, 4) == "nnh-";
}

/// The rule `name` breaks, or "".
std::string broken_rule(std::string_view n) {
    if (n.empty()) return "a name is 1 to 32 characters";
    if (n.find('/') != std::string_view::npos || n.find('\\') != std::string_view::npos) {
        return "it contains a path separator (the name is a folder in this one)";
    }
    if (n.find("..") != std::string_view::npos) return "it contains '..'";
    if (n.size() >= 2 && n[1] == ':' && ((n[0] >= 'a' && n[0] <= 'z') || (n[0] >= 'A' && n[0] <= 'Z'))) {
        return "it starts with a drive letter";
    }
    if (n[0] == '-' || n[0] == '.') return "it starts with '-' or '.'";
    if (n.size() > kMaxName) return "it is longer than 32 characters";
    for (char c : n) {
        if (!name_char(c)) return "only lowercase letters, digits, '_' and '-'";
    }
    if (!lower(n[0])) return "it must start with a letter";
    if (windows_device(n)) return "it is a name Windows reserves for a device";
    if (reserved_target(n)) return "it is a target name the engine or CMake uses in a game's build";
    return "";
}

/// Lower case, every run of other characters one '-', from the first letter, at most 32 characters.
std::string sanitized(std::string_view text) {
    std::string out;
    for (char c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (!name_char(c)) c = '-';
        if (out.empty() && !lower(c)) continue;
        if (c == '-' && !out.empty() && out.back() == '-') continue;
        out += c;
    }
    if (out.size() > kMaxName) out.resize(kMaxName);
    while (!out.empty() && (out.back() == '-' || out.back() == '_'))
        out.pop_back();
    return out;
}

std::string suggestion_for(std::string_view name) {
    std::string s = sanitized(name);
    if (!s.empty() && (windows_device(s) || reserved_target(s))) s += "-game";
    return !s.empty() && s != name && broken_rule(s).empty() ? s : "";
}

/// Decodes one UTF-8 character at `i` (strict: no overlongs, surrogates or values past U+10FFFF). false =
/// invalid.
bool next_code_point(std::string_view t, std::size_t& i, std::uint32_t& cp) {
    const auto b = static_cast<unsigned char>(t[i]);
    int extra = 0;
    std::uint32_t min = 0;
    if (b < 0x80) {
        cp = b;
    } else if (b >= 0xC2 && b <= 0xDF) {
        cp = b & 0x1Fu, extra = 1, min = 0x80;
    } else if (b >= 0xE0 && b <= 0xEF) {
        cp = b & 0x0Fu, extra = 2, min = 0x800;
    } else if (b >= 0xF0 && b <= 0xF4) {
        cp = b & 0x07u, extra = 3, min = 0x10000;
    } else {
        return false;
    }
    if (i + static_cast<std::size_t>(extra) >= t.size() && extra > 0) return false; // cut short
    for (int k = 1; k <= extra; ++k) {
        const auto c = static_cast<unsigned char>(t[i + static_cast<std::size_t>(k)]);
        if ((c & 0xC0u) != 0x80u) return false;
        cp = (cp << 6) | (c & 0x3Fu);
    }
    if (extra > 0 && (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))) return false;
    i += static_cast<std::size_t>(extra) + 1;
    return true;
}

} // namespace

NameCheck check_name(std::string_view name) {
    NameCheck c;
    c.rule = broken_rule(name);
    c.ok = c.rule.empty();
    if (!c.ok) c.suggestion = suggestion_for(name);
    return c;
}

TextCheck check_text_field(std::string_view text, bool allow_empty) {
    if (text.empty()) return allow_empty ? TextCheck{true, ""} : TextCheck{false, "it is empty"};
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size();) {
        std::uint32_t cp = 0;
        if (!next_code_point(text, i, cp)) return {false, "it is not valid UTF-8"};
        if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) return {false, "it holds a control character"};
        if (cp == '"' || cp == '\\' || cp == '$' || cp == ';' || cp == '@' || cp == '`') {
            return {false, "it can't hold \" \\ $ ; @ or ` (CMake or the README would read them)"};
        }
        ++count;
    }
    if (count > kMaxText) return {false, "it is longer than 64 characters"};
    return {true, ""};
}

std::optional<std::uint32_t> parse_unique_id(std::string_view text, std::string& error) {
    std::string_view digits = text;
    if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
        digits.remove_prefix(2);
    std::uint64_t value = 0;
    bool any = false;
    for (char c : digits) {
        int d = -1;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        if (d < 0) {
            any = false;
            break;
        }
        any = true;
        if (value <= 0xFFFFFFFFull) value = value * 16 + static_cast<std::uint64_t>(d);
    }
    if (!any || digits.empty()) {
        error = "--unique-id \"" + std::string(text) + "\" is not a hex number (e.g. 0x5A3C1)";
        return std::nullopt;
    }
    if (value < kUniqueIdMin || value > kUniqueIdMax) {
        error = "--unique-id " + std::string(text) + " is outside the homebrew range 0x00300-0xF7FFF";
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value);
}

std::string format_unique_id(std::uint32_t id) {
    std::array<char, 16> text{};
    std::snprintf(text.data(), text.size(), "0x%05X", static_cast<unsigned>(id));
    return text.data();
}

} // namespace nnh::tool
