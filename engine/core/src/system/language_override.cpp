// The console remote's language (system_internal.hpp set_language_override, spec 029 FR-27): a test script's
// `language` command replaces the platform's language until `language off`, a Bye or a disconnect. A separate object,
// linked by the remote session (Debug/Development) and system_language.cpp.

#include "system/language_override.hpp"
#include "system/language_tag.hpp"
#include "system/system_internal.hpp"

namespace nnh::detail {
namespace {

char g_code[kMaxLanguageTag + 1] = {}; // empty: none

} // namespace

void set_language_override(const char* code) {
    if (code == nullptr) {
        g_code[0] = 0;
        return;
    }
    if (!language_tag_well_formed(code)) return;
    int i = 0;
    for (; i < kMaxLanguageTag && code[i] != 0; ++i) g_code[i] = code[i];
    g_code[i] = 0;
}

const char* language_override() { return g_code[0] != 0 ? g_code : nullptr; }

} // namespace nnh::detail
