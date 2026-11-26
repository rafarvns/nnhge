// Fonts per language (nnh/localization.hpp use_fonts, font; spec 029 FR-29): each role's font of the current language
// ([language.fonts], else its chain's) loaded at start and at every switch — the new ones first, then the old ones
// released, so a font both languages use keeps its slot (the font table counts references). Linked only by games that
// call use_fonts(), font() or apply().

#include <cstring>

#include "loc/loc_state.hpp"
#include "nnh/font.hpp"
#include "nnh/log.hpp"

namespace nnh {

NNH_LOG_CHANNEL_EXTERN(loc);

namespace {

using namespace detail::loc;

/// The current language's fonts, by role index. Released by stop() (nnh::shutdown() in Debug/Development), never by a
/// static destructor: at exit the engine is gone (Release skips the shutdown hooks).
struct RoleFonts {
    union {
        Font f[loc::kMaxRoles];
    };
    RoleFonts() : f{} {}
    ~RoleFonts() {} // NOLINT: deliberately empty
};
RoleFonts g_role_fonts;
Font (&g_fonts)[loc::kMaxRoles] = g_role_fonts.f;

/// The font path of `role` for `language`: its own, else its chain's ("" for none).
const char* role_path(int language, int role) {
    const LocState& s = loc_state();
    for (int c = 0; c < kMaxChain; ++c) {
        const std::uint8_t at = s.languages[language].chain[c];
        if (at == kNoIndex) break;
        std::uint32_t offset = 0;
        std::memcpy(&offset, s.manifest + s.header.off_role_fonts + (std::size_t{at} * s.header.role_count + role) * 4, 4);
        if (offset != 0) return blob(offset);
    }
    return "";
}

void switch_fonts(int language) {
    const LocState& s = loc_state();
    Font next[loc::kMaxRoles];
    for (int r = 0; language >= 0 && r < s.header.role_count; ++r) {
        const char* path = role_path(language, r);
        if (path[0] == 0) continue;
        auto f = load_font(path);
        if (f) next[r] = static_cast<Font&&>(f.value());
        else NNH_LOG_ERROR(loc, "font %s of language %s: %s", path, s.languages[language].code, f.error().message);
    }
    for (int r = 0; r < loc::kMaxRoles; ++r) g_fonts[r] = static_cast<Font&&>(next[r]); // the old ones released here
}

} // namespace

void loc::use_fonts() {
    LocState& s = loc_state();
    if (s.fonts_switch != nullptr) return;
    s.fonts_switch = &switch_fonts;
    if (s.started) switch_fonts(s.current);
}

FontRef loc::font(Key role) {
    const LocState& s = loc_state();
    if (s.fonts_switch == nullptr) use_fonts();
    const auto* hashes = reinterpret_cast<const std::uint32_t*>(s.manifest + s.header.off_roles);
    for (int r = 0; s.started && r < s.header.role_count; ++r) {
        std::uint32_t h = 0;
        std::memcpy(&h, hashes + r, 4);
        if (h == role.hash) return g_fonts[r].ref();
    }
#if !defined(NNH_FLAVOR_RELEASE)
    static std::uint32_t logged = 0;
    if (s.started && logged != role.hash) NNH_LOG_WARN(loc, "font role \"%s\" is not declared", role.name != nullptr ? role.name : "?");
    logged = role.hash;
#endif
    return FontRef{};
}

} // namespace nnh
