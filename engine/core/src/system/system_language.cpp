// nnh::system::language() (spec 029 FR-27): a test script's override while one is set, else the platform's code; "en"
// if the platform's isn't well-formed. A separate object: a game that never asks links none of it.

#include "nnh/platform/system.hpp"
#include "nnh/system.hpp"
#include "system/language_override.hpp"
#include "system/language_tag.hpp"

namespace nnh {

const char* system::language() {
    if (const char* code = detail::language_override()) return code;
    const char* code = platform::system_language();
    return detail::language_tag_well_formed(code) ? code : "en";
}

} // namespace nnh
