// nnh-assetc — NNHGE's asset converter (spec 012, decision D27): a host program run by the build.
// Stage 0 spike: only `--version`; `map <in.tmj> <out.nnhmap> ...` comes with stage 7.

#include <cstdio>
#include <cstring>

#include "core/crc32.hpp" // shared with core (pure file); the map writer will checksum with it

#ifndef NNH_ASSETC_VERSION
#error "NNH_ASSETC_VERSION is set by tools/assetc/CMakeLists.txt"
#endif

namespace {

constexpr const char* kUsage = "usage: nnh-assetc --version\n";

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
        // The check value proves the shared CRC-32 is the one core uses (0xCBF43926).
        std::printf("nnh-assetc %s (crc32 %08X)\n", NNH_ASSETC_VERSION,
                    static_cast<unsigned>(nnh::detail::crc32("123456789", 9)));
        return 0;
    }
    std::fputs(kUsage, stderr);
    return 2;
}
