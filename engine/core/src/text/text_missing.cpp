// Missing characters logged once (text_missing.hpp). Debug/Development only: Release builds never call it.

#include "text/text_missing.hpp"

#include "nnh/log.hpp"

namespace nnh {

NNH_LOG_CHANNEL(text);

#if !defined(NNH_FLAVOR_RELEASE) // Release keeps none of the texts (scripts/check_release_strings.sh)
namespace {

struct Missing {
    std::uint8_t slot = 0;
    char32_t cp = 0;
};

Missing g_logged[detail::kMaxMissingLogged];
int g_count = 0;
bool g_full_logged = false;

} // namespace

void detail::text_missing_log(int slot, char32_t cp, const char* path) {
    for (int i = 0; i < g_count; ++i)
        if (g_logged[i].slot == slot && g_logged[i].cp == cp) return;
    if (g_count == kMaxMissingLogged) {
        if (!g_full_logged) NNH_LOG_WARN(text, "more missing characters (log full)");
        g_full_logged = true;
        return;
    }
    g_logged[g_count++] = Missing{static_cast<std::uint8_t>(slot), cp};
    NNH_LOG_WARN(text, "U+%04X not in %s", static_cast<unsigned>(cp), path != nullptr ? path : "the font");
}

void detail::text_missing_reset() {
    g_count = 0;
    g_full_logged = false;
}
#else
void detail::text_missing_reset() {}
#endif

} // namespace nnh
