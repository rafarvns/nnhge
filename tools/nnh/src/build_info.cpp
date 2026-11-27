// This nnh's build information (build_info.hpp). The only source with the build-time definitions
// (tools/nnh/CMakeLists.txt): NNH_TOOL_VERSION, NNH_RELEASE_ENGINE_URL, NNH_RELEASE_ENGINE_SHA256 and, without a pin,
// NNH_TOOL_ENGINE_DIR.

#include "build_info.hpp"

#include "nnh_link/messages.hpp"
#include "remote/remote_wire.hpp"

#ifndef NNH_TOOL_VERSION
#error "NNH_TOOL_VERSION is set by tools/nnh/CMakeLists.txt"
#endif
#ifndef NNH_RELEASE_ENGINE_URL
#define NNH_RELEASE_ENGINE_URL ""
#endif
#ifndef NNH_RELEASE_ENGINE_SHA256
#define NNH_RELEASE_ENGINE_SHA256 ""
#endif
#ifndef NNH_TOOL_ENGINE_DIR
#define NNH_TOOL_ENGINE_DIR ""
#endif

namespace nnh::tool {
namespace {

const char* host_os() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

const char* host_arch() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "arm";
#endif
}

BuildInfo make() {
    BuildInfo info;
    info.version = NNH_TOOL_VERSION;
    info.remote_protocol = nnh::detail::remote::kVersion;
    info.link_protocol = nnh::link::kProtocolVersion;
    info.os = host_os();
    info.arch = host_arch();
    info.engine_url = NNH_RELEASE_ENGINE_URL;
    info.engine_sha256 = NNH_RELEASE_ENGINE_SHA256;
    if (!info.release()) info.checkout_dir = NNH_TOOL_ENGINE_DIR;
    return info;
}

} // namespace

const BuildInfo& build_info() {
    static const BuildInfo info = make();
    return info;
}

std::string version_line(const BuildInfo& info) {
    return "nnh " + info.version + " (protocol " + std::to_string(info.remote_protocol) + ", link " +
           std::to_string(info.link_protocol) + ", " + info.os + "-" + info.arch + ", " +
           (info.release() ? "release build" : "from a source checkout") + ")";
}

} // namespace nnh::tool
