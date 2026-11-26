// Fake platform: the system language (contract nnh/platform/system.hpp system_language, spec 029 FR-27), set by tests
// with platform_fake::set_system_language(); "en" after reset().

#include "fake_internal.hpp"
#include "nnh/platform/system.hpp"

namespace nnh {
namespace {

char g_language[32] = "en";
bool g_none = false; // set_system_language(nullptr): the platform answers nullptr (core must cope)

} // namespace

const char* platform::system_language() { return g_none ? nullptr : g_language; }

void platform_fake::set_system_language(const char* code) {
    g_none = code == nullptr;
    int i = 0;
    for (; code != nullptr && i < 31 && code[i] != 0; ++i) g_language[i] = code[i];
    g_language[i] = 0;
}

void platform_fake::detail::language_reset() { set_system_language("en"); }

} // namespace nnh
