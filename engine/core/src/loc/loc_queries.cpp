// Localization's queries (nnh/localization.hpp): the current language, the shipped list, find.

#include "loc/loc_state.hpp"
#include "system/language_tag.hpp"

namespace nnh {

using detail::loc::loc_state;

const char* loc::code() { return loc_state().current >= 0 ? loc_state().languages[loc_state().current].code : ""; }

const char* loc::name() { return language_name(loc_state().current); }

int loc::index() { return loc_state().current; }

std::uint32_t loc::serial() { return loc_state().serial; }

int loc::language_count() { return loc_state().languages != nullptr ? loc_state().header.language_count : 0; }

const char* loc::language_code(int i) {
    return i >= 0 && i < language_count() ? loc_state().languages[i].code : "";
}

const char* loc::language_name(int i) {
    return i >= 0 && i < language_count() ? detail::loc::blob(loc_state().languages[i].name) : "";
}

int loc::find(const char* code) {
    for (int i = 0; i < language_count() && code != nullptr; ++i)
        if (detail::same_language_tag(code, loc_state().languages[i].code)) return i;
    return -1;
}

} // namespace nnh
