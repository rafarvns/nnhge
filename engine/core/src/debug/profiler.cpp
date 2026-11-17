// Frame profiler (nnh/profile.hpp): fixed tables, keyed by the scope name's address.

#include "nnh/profile.hpp"

#include "debug/dev_tools.hpp"
#include "nnh/platform/time.hpp"

namespace nnh::profile {
namespace {

struct Table {
    ScopeStats scopes[kMaxScopes];
    int count = 0;
};

Table g_current;
Table g_previous;

ScopeStats* find_or_add(const char* name) {
    for (int i = 0; i < g_current.count; ++i) {
        if (g_current.scopes[i].name == name) return &g_current.scopes[i];
    }
    if (g_current.count == kMaxScopes) return nullptr;
    ScopeStats& s = g_current.scopes[g_current.count++];
    s = ScopeStats{name, Duration{}, 0};
    return &s;
}

} // namespace

Scope::Scope(const char* name) : name_(name), start_ns_(platform::ticks_ns()) {}

Scope::~Scope() {
    ScopeStats* s = find_or_add(name_);
    if (s == nullptr) return;
    s->total += Duration{platform::ticks_ns() - start_ns_};
    ++s->calls;
}

int slowest(ScopeStats* out, int max) {
    int n = 0;
    for (int i = 0; i < g_previous.count && n < max; ++i) out[n++] = g_previous.scopes[i];
    // Insertion sort by total time, descending (n is small).
    for (int i = 1; i < n; ++i) {
        const ScopeStats key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].total < key.total) {
            out[j + 1] = out[j];
            --j;
        }
        out[j + 1] = key;
    }
    return n;
}

} // namespace nnh::profile

void nnh::detail::profile_frame_boundary() {
    profile::g_previous = profile::g_current;
    profile::g_current.count = 0;
}

void nnh::detail::profile_reset() {
    profile::g_previous.count = 0;
    profile::g_current.count = 0;
}
