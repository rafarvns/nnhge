// PC simulator: the system language (contract nnh/platform/system.hpp system_language, spec 029 FR-27):
// NNH_SIM_LANGUAGE if well-formed, else `language=` in language.ini beside the game's window.ini (only when the
// simulator remembers its view: never for scripted runs), else "en" — so screenshots don't depend on the host machine.
// Read once.

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstring>

#include "context_pc.hpp"
#include "language_pc.hpp"
#include "nnh/platform/system.hpp"
#include "system/language_tag.hpp"

namespace nnh::platform {
namespace {

bool space(char c) { return c == ' ' || c == '\t' || c == '\r'; }

/// One line [p, end): `language = code` → out.
bool line_language(const char* p, const char* end, char out[16]) {
    while (p < end && space(*p)) ++p;
    while (end > p && space(end[-1])) --end;
    constexpr char kKey[] = "language";
    const std::size_t key = sizeof kKey - 1;
    if (static_cast<std::size_t>(end - p) <= key || std::memcmp(p, kKey, key) != 0) return false;
    p += key;
    while (p < end && space(*p)) ++p;
    if (p == end || *p != '=') return false;
    ++p;
    while (p < end && space(*p)) ++p;
    const std::size_t n = static_cast<std::size_t>(end - p);
    if (n == 0 || n > static_cast<std::size_t>(detail::kMaxLanguageTag)) return false;
    char code[16] = {};
    std::memcpy(code, p, n);
    if (!detail::language_tag_well_formed(code)) return false;
    std::memcpy(out, code, 16);
    return true;
}

/// language.ini in the folder of `settings_path` (window.ini), read bounded.
bool language_from_file(const char* settings_path, char out[16]) {
    const char* slash = std::strrchr(settings_path, '/');
    const char* back = std::strrchr(settings_path, '\\');
    if (back != nullptr && (slash == nullptr || back > slash)) slash = back;
    if (slash == nullptr) return false;
    char path[600];
    const auto folder = static_cast<std::size_t>(slash - settings_path + 1);
    if (folder + sizeof "language.ini" > sizeof path) return false;
    std::memcpy(path, settings_path, folder);
    std::memcpy(path + folder, "language.ini", sizeof "language.ini");
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (io == nullptr) return false;
    char text[pc::kMaxLanguageIniBytes];
    const std::size_t got = SDL_ReadIO(io, text, sizeof text);
    SDL_CloseIO(io);
    return pc::language_from_ini(text, got, out);
}

} // namespace

bool pc::language_from_ini(const char* text, std::size_t size, char out[16]) {
    if (text == nullptr || size > kMaxLanguageIniBytes) return false;
    const char* end = text + size;
    for (const char* line = text; line < end;) {
        const char* next = line;
        while (next < end && *next != '\n') ++next;
        const bool ok_length = static_cast<std::size_t>(next - line) <= kMaxLanguageIniLine;
        if (ok_length && *line != '#' && *line != ';' && line_language(line, next, out)) return true;
        line = next < end ? next + 1 : end;
    }
    return false;
}

const char* system_language() {
    static char code[16] = {};
    if (code[0] != 0) return code;
    const char* env = std::getenv("NNH_SIM_LANGUAGE");
    if (env != nullptr && std::strlen(env) <= static_cast<std::size_t>(detail::kMaxLanguageTag) &&
        detail::language_tag_well_formed(env)) {
        std::memcpy(code, env, std::strlen(env) + 1);
    } else if (pc::context().settings_path[0] == 0 || !language_from_file(pc::context().settings_path, code)) {
        std::memcpy(code, "en", 3);
    }
    return code;
}

} // namespace nnh::platform
