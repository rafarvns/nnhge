#pragma once
// Arguments for Luma3DS's homebrew loader service `hb:ldr` (spec 011 FR-4). Pure code, no libctru: also
// compiled into nnh_tests on PC. Facts from the Homebrew Launcher (source/loaders/rosalina.c,
// launch.c) and Luma3DS (sysmodules/loader/source/hbldr.c):
// - SetTarget takes the path without the "sdmc:" prefix (the loader opens it on the SD card);
// - SetArgv copies a 0x400-byte block: a 32-bit argument count, then NUL-terminated strings, argv[0]
//   being the full "sdmc:/..." path;
// - the target is cleared after each load (the next load is /boot.3dsx), argv is not.

#include <cstddef>
#include <cstdint>

namespace nnh::platform::ctr {

constexpr std::size_t kHbldrArgvSize = 0x400;
constexpr std::size_t kHbldrMaxPath = 255;

struct HbldrArgv {
    std::uint32_t words[kHbldrArgvSize / 4];
};

/// Fills `out` with argv = {path, args[0..count)}. False if an argument is null or the block overflows.
bool build_hbldr_argv(const char* path, const char* const* args, int count, HbldrArgv& out);

/// "sdmc:/3ds/x.3dsx" → "/3ds/x.3dsx" (a pointer into `path`); nullptr unless `path` names a .3dsx on the SD
/// card (no empty or ".." segments, at most kHbldrMaxPath characters).
const char* hbldr_target(const char* path);

/// The path of the last "--nnh-return=<path>" among argv[1..argc) that hbldr_target() accepts, or nullptr.
const char* return_path(int argc, char* const* argv);

} // namespace nnh::platform::ctr
