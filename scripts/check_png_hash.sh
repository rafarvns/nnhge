#!/bin/sh
# Renderer-independence of simulator screenshots (spec 026 FR-23, AC-5): runs a game three ways — the simulator's
# default renderer (GPU device when available) in a window, SDL's plain renderer (NNH_SIM_RENDERER=sdl, software) and
# SDL's offscreen video driver — and prints the screenshot's SHA-256 for each. Exit 0 when all three are equal; with a
# golden hash given, also when it matches.
#
#   scripts/check_png_hash.sh <game.exe> <frames> [golden] [VAR=value ...]
#   scripts/check_png_hash.sh build/pc-debug/examples/models/models.exe 60 "" MODELS_DEMO=showcase

exe=$1
frames=$2
golden=$3
shift 3
mkdir -p build/png-hash
status=0
first=""
for mode in window sdl offscreen; do
    case $mode in
    window) set -- "$@" ;;
    esac
    png=build/png-hash/$mode.png
    rm -f "$png"
    case $mode in
    window) env "$@" NNH_EXIT_AFTER_FRAMES="$frames" NNH_SCREENSHOT="$png" SDL_AUDIO_DRIVER=dummy "$exe" > /dev/null 2>&1 ;;
    sdl) env "$@" NNH_SIM_RENDERER=sdl SDL_RENDER_DRIVER=software NNH_EXIT_AFTER_FRAMES="$frames" NNH_SCREENSHOT="$png" \
             SDL_AUDIO_DRIVER=dummy "$exe" > /dev/null 2>&1 ;;
    offscreen) env "$@" SDL_VIDEO_DRIVER=offscreen NNH_EXIT_AFTER_FRAMES="$frames" NNH_SCREENSHOT="$png" \
                   SDL_AUDIO_DRIVER=dummy "$exe" > /dev/null 2>&1 ;;
    esac
    hash=$( [ -f "$png" ] && sha256sum "$png" | cut -c1-64 || echo "(no screenshot)")
    echo "$mode $hash"
    [ -z "$first" ] && first=$hash
    [ "$hash" = "$first" ] || status=1
    [ -n "$golden" ] && [ "$hash" != "$golden" ] && status=1
done
[ $status -eq 0 ] && echo "png hash: SAME" || echo "png hash: DIFFERENT"
exit $status
