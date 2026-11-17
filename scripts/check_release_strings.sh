#!/usr/bin/env sh
# Fails if a Release binary contains strings that only Debug/Development builds should have
# (info/debug log messages, assert-only text). Spec 002 AC-5.
#
#   scripts/check_release_strings.sh build/3ds-release/examples/hello/hello.elf
set -eu

binary="$1"
# Format strings of NNH_LOG_INFO calls in the engine; they must be compiled out in Release.
forbidden='shutdown after %llu frames|started "%s" \(%s build\)'

if grep -aEq "$forbidden" "$binary"; then
    echo "FAIL: $binary contains debug-only strings:"
    grep -aEo "$forbidden" "$binary" | sort -u
    exit 1
fi
echo "ok: $binary has no debug-only strings ($(wc -c < "$binary") bytes)"
