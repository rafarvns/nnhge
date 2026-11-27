#!/bin/sh
# The longest command of a build folder against the Windows command-line limit (spec 032 FR-19, AC-11).
#
#   scripts/check_command_length.sh <build dir> [limit] [target...]
#
# Reads Ninja's command list (`ninja -C <dir> -t commands [target...]`; no target: the default build, which leaves out
# `dist` and `<game>_cia`: name them to measure packaging too): every command the build would run, whole lines (an
# archive's `rm`, `ar` and `ranlib` together, as Ninja hands them to the shell). Prints the longest one's length, its
# share of 32,767 (the Windows limit of one command line) and its first 100 characters.
# Exit: 0 at or below the limit (default 16,383 = 50 % of 32,767), 1 above it, 2 without a build.ninja.
# 3DS presets on Windows are what this guards: devkitPro's MSYS2 CMake assumes the POSIX limit and writes no response
# files, so a command over 32,767 characters fails with "Argument list too long" (D55 item 9).
set -eu

if [ $# -lt 1 ]; then
    echo "usage: scripts/check_command_length.sh <build dir> [limit] [target...]" >&2
    exit 2
fi
dir=$1
limit=${2:-16383}
shift
[ $# -gt 0 ] && shift
if [ ! -f "$dir/build.ninja" ]; then
    echo "check_command_length: no build.ninja in $dir" >&2
    exit 2
fi
ninja_program=${NINJA:-ninja}
"$ninja_program" -C "$dir" -t commands "$@" | awk -v limit="$limit" -v dir="$dir" '
    { n = length($0); if (n > max) { max = n; head = substr($0, 1, 100) } }
    END {
        if (max == 0) { printf "check_command_length: %s: ninja listed no command\n", dir; exit 2 }
        share = max * 100.0 / 32767
        printf "check_command_length: %s: longest command %d characters (%.1f %% of 32767; limit %d)\n", dir, max, share, limit
        printf "  %s...\n", head
        if (max > limit) { print "check_command_length: FAIL: above the limit"; exit 1 }
        print "check_command_length: PASS"
    }'
