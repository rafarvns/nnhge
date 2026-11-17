#pragma once
/// @file nnh/build.hpp
/// The build flavor of this binary: Debug, Development or Release. Chosen by the preset
/// (`*-debug`, `*-dev`, `*-release`); see docs/api/build.md.
///
/// Engine headers use these values to compile asserts, verbose logs and developer tools out of
/// Release builds. Games may *show* the flavor (e.g. a "dev build" label) but should not change
/// game behaviour with it.

// CMake defines exactly one NNH_FLAVOR_* on nnh::core from the build type. A file compiled without
// any (e.g. a quick test outside CMake) is treated as Development.
#if defined(NNH_FLAVOR_DEBUG) + defined(NNH_FLAVOR_DEVELOPMENT) + defined(NNH_FLAVOR_RELEASE) > 1
#error "More than one NNH_FLAVOR_* is defined"
#endif
#if !defined(NNH_FLAVOR_DEBUG) && !defined(NNH_FLAVOR_DEVELOPMENT) && !defined(NNH_FLAVOR_RELEASE)
#define NNH_FLAVOR_DEVELOPMENT 1
#endif

namespace nnh::build {

enum class Flavor {
    Debug,       ///< no optimization, every check and tool on — for stepping in a debugger
    Development, ///< optimized with debug info, checks and tools on — daily work, testing on 3DS
    Release,     ///< optimized, only NNH_VERIFY checks, no developer tools — what players get
};

/// Flavor of this build.
constexpr Flavor flavor() {
#if defined(NNH_FLAVOR_DEBUG)
    return Flavor::Debug;
#elif defined(NNH_FLAVOR_DEVELOPMENT)
    return Flavor::Development;
#else
    return Flavor::Release;
#endif
}

/// "Debug", "Development" or "Release".
constexpr const char* flavor_name() {
    switch (flavor()) {
    case Flavor::Debug: return "Debug";
    case Flavor::Development: return "Development";
    case Flavor::Release: return "Release";
    }
    return "Unknown";
}

/// NNH_ASSERT is active (Debug and Development).
inline constexpr bool kAssertsEnabled = flavor() != Flavor::Release;

/// Debug overlay, profiler and simulator developer controls are compiled in (Debug and Development).
inline constexpr bool kDevToolsEnabled = flavor() != Flavor::Release;

} // namespace nnh::build
