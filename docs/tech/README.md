# Technology notes

What we use, where, and the rules that matter for this project. Facts here are checked against the
installed toolchain headers, not written from memory; when a version changes, re-check.

| Technology | Used for | Where in the tree | Notes |
|------------|----------|-------------------|-------|
| devkitPro (devkitARM, libctru, citro2d, citro3d) | 3DS backend, `.3dsx`/SMDH | `engine/src/platform/3ds/` | [devkitpro-3ds.md](devkitpro-3ds.md) |
| CMake + Ninja (+ ccache) | Build system for every target | `CMakeLists.txt`, `cmake/`, `CMakePresets.json` | [cmake.md](cmake.md) |
| C++20 (engine subset) | All engine and tool code | everywhere | [cpp.md](cpp.md) |
| SDL 3.4 | PC simulator backend | `engine/src/platform/pc/` | [sdl3.md](sdl3.md) |
| GitHub Actions | CI (Linux PC + 3DS container) | `.github/workflows/ci.yml` | [cmake.md](cmake.md#ci) |

Versions in use (2026-11-17): devkitARM r68 (GCC 16.1), libctru 2.7.1, citro2d 1.7.0, citro3d 1.7.1, 3dstools 1.3.1,
CMake 4.x, SDL 3.4.20, host GCC 16 (WinLibs).
