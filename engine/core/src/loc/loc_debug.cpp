// Localization's debugging (nnh/localization.hpp dump, set_show_keys; spec 029 FR-33): Debug/Development only.

#include "loc/loc_state.hpp"
#include "nnh/log.hpp"

namespace nnh {

NNH_LOG_CHANNEL_EXTERN(loc);

void loc::dump() {
#if !defined(NNH_FLAVOR_RELEASE)
    const detail::loc::LocState& s = detail::loc::loc_state();
    if (!s.started) {
        NNH_LOG_INFO(loc, "loc: not started");
        return;
    }
    std::uint32_t keys = 0, bytes = 0;
    for (int t = 0; t < s.table_count; ++t) {
        keys += s.tables[t].h.key_count;
        bytes += s.tables[t].h.size;
    }
    NNH_LOG_INFO(loc, "loc: %s (%s), %d languages, chain of %d tables: %u keys, %u bytes (block %zu), serial %u",
                 code(), name(), language_count(), s.table_count, static_cast<unsigned>(keys),
                 static_cast<unsigned>(bytes), s.block_bytes, static_cast<unsigned>(s.serial));
#endif
}

void loc::set_show_keys([[maybe_unused]] bool on) {
#if !defined(NNH_FLAVOR_RELEASE)
    detail::loc::loc_state().show_keys = on;
#endif
}

} // namespace nnh
