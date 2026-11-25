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
# The simulator's end (spec 013 stage 4): NNH_REMOTE is read, and the log redirected, only in Debug/Development.
forbidden="$forbidden"'|NNH_REMOTE|remote_open|remote_log_line|parse_remote_address'
# Input actions (spec 017 FR-9, FR-24): the Development warning about unusable actions (the debug view is compiled
# out with it).
forbidden="$forbidden"'|unusable on this system'
# Structured saves (spec 027 FR-17): the dump's texts and NNH_SAVE_DUMP, and saved member names (kept only in
# Debug/Development; the saves example's `play_frames` and `best_time` stand for them). Check a game that saves on its
# .3dsx or .exe: a 3DS .elf keeps DWARF, whose type descriptions name every struct member.
forbidden="$forbidden"'|\(not 0/1\)|\(retired\) |NNH_SAVE_DUMP|play_frames|best_time'
# UI widgets (spec 028 FR-30, FR-32): the dump's and overflow warning's texts and the misuse messages (Release keeps one
# generic line per UI).
forbidden="$forbidden"'|ui dump:|overflows: its content|a stale or null|the UI is full|a key used twice|Ui::set_'

if grep -aEq "$forbidden" "$binary"; then
    echo "FAIL: $binary contains debug-only strings:"
    grep -aEo "$forbidden" "$binary" | sort -u
    exit 1
fi
echo "ok: $binary has no debug-only strings ($(wc -c < "$binary") bytes)"
