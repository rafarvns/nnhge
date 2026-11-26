// nnh::system::language() (spec 029 FR-27): a test script's override while one is set (and well-formed), else the
// platform's code; "en"
// if the platform's isn't well-formed. A separate object: a game that never asks links none of it.

#include "nnh/platform/system.hpp"
#include "nnh/system.hpp"
#include "system/language_override.hpp"
#include "system/language_tag.hpp"

namespace nnh {

const char* system::language() {
    const char* forced = detail::language_override();
    if (forced != nullptr && detail::language_tag_valid(forced)) return forced;
    const char* code = platform::system_language();
    return detail::language_tag_valid(code) ? code : "en";
}

} // namespace nnh
