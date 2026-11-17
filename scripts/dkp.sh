#!/usr/bin/env sh
# Runs a command inside the devkitPro environment, from the repository root.
#
#   scripts/dkp.sh cmake --preset 3ds-release
#   scripts/dkp.sh cmake --build --preset 3ds-release
#
# On Windows, devkitPro's CMake toolchain only works from devkitPro's own MSYS2, so the command runs
# there (override the location with DEVKITPRO_MSYS2). On Linux/macOS it runs directly.
set -eu

repo=$(cd "$(dirname "$0")/.." && pwd)

case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
    dkp_bash="${DEVKITPRO_MSYS2:-/c/devkitPro/msys2}/usr/bin/bash.exe"
    if [ ! -x "$dkp_bash" ]; then
        echo "devkitPro MSYS2 not found at $dkp_bash (set DEVKITPRO_MSYS2)" >&2
        exit 1
    fi
    exec "$dkp_bash" -lc 'cd "$0" && exec "$@"' "$repo" "$@"
    ;;
*)
    cd "$repo"
    exec "$@"
    ;;
esac
