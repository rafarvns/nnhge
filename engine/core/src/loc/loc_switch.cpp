// Switching languages (nnh/localization.hpp; spec 029 FR-25, FR-27, FR-28): set_language() records the choice; the
// after-input hook (D32, registered by start()) applies it at the start of the next frame — the block overwritten with
// the new chain (a failure reloads the previous one and publishes nothing), the role fonts swapped, LanguageChanged
// published (delivered in that frame's dispatch) — and follows system::language() when asked.

#include <cstring>

#include "loc/loc_state.hpp"
#include "nnh/assert.hpp"
#include "nnh/log.hpp"
#include "nnh/profile.hpp"
#include "nnh/system.hpp"

namespace nnh {

NNH_LOG_CHANNEL_EXTERN(loc);

void detail::loc::loc_frame_hook(InputHook&) {
    LocState& s = loc_state();
    if (!s.started) return;
    if (s.follow) {
        const char* system = system::language();
        if (std::strncmp(system, s.system_code, sizeof s.system_code - 1) != 0) {
            std::strncpy(s.system_code, system, sizeof s.system_code - 1);
            int match = match_language(system);
            if (match < 0 || match == s.header.pseudo) match = s.header.base; // the pseudo-language only when chosen
            if (match != s.current) {
                s.pending = match;
                s.pending_why = "system";
            }
        }
    }
    if (s.pending < 0) return;
    const int previous = s.current, target = s.pending;
    s.pending = -1;
    if (target == previous) return;
    NNH_PROFILE_SCOPE("loc switch"); // the bench's switch phase (Development); nothing in Release
    if (const char* why = load_chain(target)) {
        NNH_LOG_ERROR(loc, "language %s: %s (staying in %s)", s.languages[target].code, why, s.languages[previous].code);
        if (load_chain(previous) != nullptr) NNH_LOG_ERROR(loc, "language %s can't be reloaded", s.languages[previous].code);
        return;
    }
    announce(previous, target, s.pending_why);
    if (s.fonts_switch != nullptr) s.fonts_switch(target);
}

Result<void> loc::set_language(const char* code) {
    detail::loc::LocState& s = detail::loc::loc_state();
    if (!s.started) {
        NNH_ASSERT(false, "loc::set_language before loc::start()");
        return Error::make(ErrorCode::InvalidArgument, "loc::set_language before loc::start()");
    }
    const int i = find(code);
    if (i < 0) {
        NNH_LOG_WARN(loc, "set_language(\"%s\"): not a shipped language", code != nullptr ? code : "");
        return Error::make(ErrorCode::NotFound, "language \"%s\" isn't shipped", code != nullptr ? code : "");
    }
    s.pending = i;
    s.pending_why = "player";
    s.follow = false;
    return Result<void>{};
}

void loc::follow_system(bool on) {
    detail::loc::LocState& s = detail::loc::loc_state();
    s.follow = on;
    if (on) s.system_code[0] = 0; // the next frame compares and switches if it must
}

} // namespace nnh
