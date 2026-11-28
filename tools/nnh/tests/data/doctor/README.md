# Recorded tool outputs

What `nnh doctor` reads from each tool, for `test_tool_versions.cpp` (spec 033 FR-19, AC-6).

| File | Source |
|---|---|
| `cmake-4.4.2-winlibs.txt`, `ninja-1.13-winlibs.txt`, `gcc-16.2-winlibs.txt` | recorded on the maintainer's Windows machine (WinLibs GCC 16.2, 2026-11-27) |
| `cmake-4.0.2-devkitpro-msys2.txt`, `arm-none-eabi-gcc-devkitarm-r68.txt` | recorded from devkitPro (MSYS2 CMake 4.0.2, devkitARM r68) on the same machine |
| `gcc-13.3-ubuntu.txt`, `gcc-12.4-ubuntu.txt`, `clang-16.0-ubuntu.txt`, `cmake-3.28.3-ubuntu.txt` | the first lines as printed on the build VM (Ubuntu 24.04), copied from the T0.3 runs |
| `clang-18.1-ubuntu.txt`, `ninja-1.11.1-ubuntu.txt` | Ubuntu 24.04's packages, in their documented format |
| `cmake-3.25.1.txt`, `cmake-3.20.0.txt` | CMake's `--version` format with those numbers (the minimum, and one below it) |
| `apple-clang-15.0.txt`, `msvc-cl-19.44.txt` | samples in the format Apple clang and MSVC `cl` print (neither is installed here; `cl` prints its banner on stderr, with CRLF) |
