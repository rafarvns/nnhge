#!/usr/bin/env sh
# Runs a command inside the devkitPro environment, from the repository root.
#
#   scripts/dkp.sh cmake --preset 3ds-release
#   scripts/dkp.sh cmake --build --preset 3ds-release
#
# On Windows, devkitPro's CMake toolchain only works from devkitPro's own MSYS2, so the command runs
# there (override the location with DEVKITPRO_MSYS2). On Linux/macOS it runs directly.
#
# Host tools (nnh-assetc, cmake/nnh_host_tools.cmake) need a host C++ compiler, and devkitPro's MSYS2
# has none (its login shell also resets PATH). So on Windows this passes the caller's compiler in
# NNH_HOST_CXX, as a Windows path: an existing NNH_HOST_CXX wins, else g++/c++/clang++ from this PATH
# (WinLibs MinGW on the maintainer's machine). CMake reads it once, when it configures.
set -eu

repo=$(cd "$(dirname "$0")/.." && pwd)

case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
    dkp_bash="${DEVKITPRO_MSYS2:-/c/devkitPro/msys2}/usr/bin/bash.exe"
    if [ ! -x "$dkp_bash" ]; then
        echo "devkitPro MSYS2 not found at $dkp_bash (set DEVKITPRO_MSYS2)" >&2
        exit 1
    fi
    host_cxx="${NNH_HOST_CXX:-}"
    if [ -z "$host_cxx" ]; then
        host_cxx=$(command -v g++ || command -v c++ || command -v clang++ || true)
        if [ -n "$host_cxx" ] && [ -f "$host_cxx.exe" ]; then host_cxx="$host_cxx.exe"; fi
    fi
    if [ -n "$host_cxx" ]; then host_cxx=$(cygpath -m "$host_cxx"); fi
    # The environment does not reach devkitPro's MSYS2 from Git Bash (different MSYS runtimes; the child
    # starts with a handful of variables), so the value travels as an argument, as a Windows path, and is
    # exported inside in MSYS form (/c/...): MSYS's CMake misreads "C:/..." as a list.
    exec "$dkp_bash" -lc 'cd "$0" && NNH_HOST_CXX=${1:+$(cygpath -u "$1")} && export NNH_HOST_CXX && shift && exec "$@"' \
        "$repo" "$host_cxx" "$@"
    ;;
*)
    cd "$repo"
    exec "$@"
    ;;
esac
