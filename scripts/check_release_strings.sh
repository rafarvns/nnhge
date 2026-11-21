#!/usr/bin/env sh
# Fails if a Release binary contains strings that only Debug/Development builds should have
# (info/debug log messages, assert-only text) or the console remote's code (spec 002 AC-5, spec 013 AC-4).
#
#   scripts/check_release_strings.sh build/3ds-release/examples/hello/hello.elf
#   scripts/check_release_strings.sh build/pc-release/examples/hello/hello.exe
set -eu

binary="$1"
# Format strings of NNH_LOG_INFO calls in the engine; they must be compiled out in Release.
forbidden='shutdown after %llu frames|started "%s" \(%s build\)'
# Console remote (spec 013): only Debug/Development builds may be driven from the PC. Its log texts, and the
# names of its hooks and platform functions (found when the binary keeps symbols: the .elf, MinGW .exe).
forbidden="$forbidden"'|remote control on|remote input off|its time is not representative|could not be read back'
forbidden="$forbidden"'|remote_frame_start|remote_frame_ended|remote_receive|remote_send|remote_capture|screenshots_send'

if grep -aEq "$forbidden" "$binary"; then
    echo "FAIL: $binary contains debug-only strings:"
    grep -aEo "$forbidden" "$binary" | sort -u
    exit 1
fi
echo "ok: $binary has no debug-only strings ($(wc -c < "$binary") bytes)"
