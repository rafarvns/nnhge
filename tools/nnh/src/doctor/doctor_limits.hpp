#pragma once
// nnh doctor's minimums and thresholds (spec 033 FR-19, Q10, Q11; the same table in docs/guides/nnh-tool.md).
// Compilers: FAIL below the oldest that built the engine and nnh with 0 warnings (VM 120, plan 033 T0.3:
// GCC 12.4, clang 16.0 and 17.0), warn below the versions the project checks on every spec (GCC 13 and 14 on
// VM 120, WinLibs GCC 16 on Windows, clang 18 on VM 120). Disk: one PC + one 3DS build of a new project ≈ 81
// MB, all four ≈ 116 MB (plan 033 T4.4).

#include "doctor/tool_versions.hpp"

#include <cstdint>

namespace nnh::tool::limits {

inline constexpr Version kCmake{3, 25, 0};          ///< CMake (the presets' schema): FAIL below
inline constexpr Version kDevkitProCmake{3, 25, 0}; ///< CMake inside devkitPro's MSYS2: FAIL below
inline constexpr Version kGccMin{12, 0, 0};         ///< FAIL below
inline constexpr Version kGccChecked{13, 0, 0};     ///< warn below
inline constexpr Version kClangMin{16, 0, 0};
inline constexpr Version kClangChecked{18, 0, 0};
/// MSVC: provisional (no MSVC build is checked): FAIL below, warn at or above (best effort).
inline constexpr Version kMsvcMin{19, 30, 0};
inline constexpr std::int64_t kDiskFailBytes = 100ll * 1000 * 1000; ///< FAIL below (one PC + one 3DS build)
inline constexpr std::int64_t kDiskWarnBytes = 350ll * 1000 * 1000; ///< warn below (≈ 3× a project's builds)

} // namespace nnh::tool::limits
