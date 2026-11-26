// The console remote's language (system_internal.hpp set_language_override, spec 029 FR-27): a test script's
// `language` command replaces the platform's language until `language off`, a Bye or a disconnect. A separate object,
// linked by the remote session (Debug/Development) and system_language.cpp.

#include "system/language_override.hpp"
#include "system/system_internal.hpp"

namespace nnh::detail {
namespace {

constexpr int kMaxCode = 15;
char g_code[kMaxCode + 1] = {}; // empty: none

} // namespace

void set_language_override(const char* code) {
    if (code == nullptr) {
        g_code[0] = 0;
        return;
    }
    int i = 0; // copied as it is: system::language() checks it (a Debug/Development game links no tag rules here)
    for (; i < kMaxCode && code[i] != 0; ++i) g_code[i] = code[i];
    g_code[i] = 0;
}

const char* language_override() { return g_code[0] != 0 ? g_code : nullptr; }

} // namespace nnh::detail
