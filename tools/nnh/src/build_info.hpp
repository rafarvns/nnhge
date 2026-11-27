#pragma once
// What this nnh was built from (spec 033 FR-1, D56 item 6): the engine version, the protocol versions it speaks, the
// host it was built for and how it was built. A release nnh (spec 034) is built with the engine archive's URL and
// SHA-256 (the CMake cache entries NNH_RELEASE_ENGINE_URL and NNH_RELEASE_ENGINE_SHA256), which `nnh new` writes as a
// new project's pin; any other nnh is a checkout build and knows the checkout it was built from. The definitions reach
// build_info.cpp only, so changing them recompiles one file. Tested in tools/nnh/tests/test_build_info.cpp.

#include <string>

namespace nnh::tool {

struct BuildInfo {
    std::string version;       ///< the engine version nnh was built from ("0.1.0", NNHGE_VERSION)
    int remote_protocol = 0;   ///< the game<->tool protocol (spec 013, remote_wire.hpp kVersion)
    int link_protocol = 0;     ///< NNH Link's protocol (spec 011, nnh_link/messages.hpp kProtocolVersion)
    std::string os;            ///< "windows", "linux" or "macos"
    std::string arch;          ///< "x64", "x86", "arm64" or "arm"
    std::string engine_url;    ///< the release pin's archive URL ("" = no pin)
    std::string engine_sha256; ///< the release pin's SHA-256 (64 hex digits, "" = no pin)
    std::string checkout_dir;  ///< the engine checkout nnh was built from; "" in a release build

    /// "Release build" means exactly "has a pin" (D56 item 3): both values set.
    bool release() const { return !engine_url.empty() && !engine_sha256.empty(); }
};

/// This nnh's build information (compiled in).
const BuildInfo& build_info();

/// `nnh <version> (protocol <remote>, link <link>, <os>-<arch>, <release build | from a source checkout>)`.
std::string version_line(const BuildInfo& info);

} // namespace nnh::tool
