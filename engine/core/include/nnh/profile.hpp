#pragma once
/// @file nnh/profile.hpp
/// Lightweight frame profiler: time named scopes, see the slowest ones in the debug overlay.
/// Compiled out in Release. See docs/api/profile.md.
///
/// @code
/// void update() {
///     NNH_PROFILE_SCOPE("update");
///     ...
/// }
/// @endcode

#include "nnh/build.hpp"
#include "nnh/time.hpp"

namespace nnh::profile {

inline constexpr int kMaxScopes = 64; ///< distinct scope names per frame; extra ones are ignored

struct ScopeStats {
    const char* name;
    Duration total; ///< time spent in the scope during the previous frame (nested scopes included)
    int calls;
};

/// The previous frame's scopes, slowest first. @return how many were written to `out`.
int slowest(ScopeStats* out, int max);

/// RAII timer used by NNH_PROFILE_SCOPE. `name` must be a string literal (its address is the key).
class Scope {
public:
    explicit Scope(const char* name);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    const char* name_;
    std::int64_t start_ns_;
};

} // namespace nnh::profile

#define NNH_PROFILE_CONCAT_(a, b) a##b
#define NNH_PROFILE_CONCAT(a, b) NNH_PROFILE_CONCAT_(a, b)
#if defined(NNH_FLAVOR_RELEASE)
#define NNH_PROFILE_SCOPE(name) ((void)0)
#else
/// Times the enclosing scope under `name` (a string literal).
#define NNH_PROFILE_SCOPE(name) ::nnh::profile::Scope NNH_PROFILE_CONCAT(nnh_profile_scope_, __LINE__)(name)
#endif
