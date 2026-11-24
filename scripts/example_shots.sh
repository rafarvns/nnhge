#!/bin/sh
# Screenshot hashes of every example (spec 026 T0.2/T9.4, AC-9): runs each built example headless for N frames
# (default 30) and prints "<example> <sha256>" per PNG. Compare two runs to prove 2D frames stay byte-identical.
#
#   scripts/example_shots.sh [preset] [frames] [out-dir]     # defaults: pc-debug 30 build/shots-<preset>
#
# Headless: SDL's offscreen video driver, its software renderer and the simulator's plain SDL renderer, dummy audio.

preset=${1:-pc-debug}
frames=${2:-30}
out=${3:-build/shots-$preset}
mkdir -p "$out"
for exe in build/"$preset"/examples/*/*; do
    [ -f "$exe" ] && [ -x "$exe" ] || continue
    name=$(basename "$exe" .exe)
    case "$exe" in *.dll | *.stamp | *.cmake | *.png) continue ;; esac
    [ "$(basename "$(dirname "$exe")")" = "$name" ] || continue
    rm -f "$out/$name.png"
    SDL_VIDEO_DRIVER=offscreen SDL_RENDER_DRIVER=software NNH_SIM_RENDERER=sdl SDL_AUDIO_DRIVER=dummy \
        NNH_EXIT_AFTER_FRAMES=$frames NNH_SCREENSHOT="$out/$name.png" "$exe" > "$out/$name.log" 2>&1
    if [ -f "$out/$name.png" ]; then
        echo "$name $(sha256sum "$out/$name.png" | cut -c1-64)"
    else
        echo "$name (no screenshot)"
    fi
done | sort -u
