#!/bin/sh
# A copy of the consumer game project (tests/consumer) outside the engine repository (spec 032 T5.1, T8.1, T9.1).
#
#   tests/consumer_tools/copy_project.sh <dest> [pin|local]
#
# pin (default): also packs this engine folder as <dest>/../nnhge-engine/nnhge.tar.gz and writes <dest>/engine_pin.cmake
#                (its file:// URL and SHA-256): the copy downloads the engine like a game pinning a release.
# local:         no pin; set NNHGE_ENGINE_DIR to an engine checkout before building (the presets map it).
# Then, from <dest>: nnh build nnh_consumer [--release] / nnh run nnh_consumer --pc --script game/tests/check.nnhs.
set -eu
if [ $# -lt 1 ]; then
    echo "usage: tests/consumer_tools/copy_project.sh <dest> [pin|local]" >&2
    exit 2
fi
dest=$1
mode=${2:-pin}
tools=$(cd "$(dirname "$0")" && pwd)
engine=$(cd "$tools/../.." && pwd)
mkdir -p "$dest"
dest=$(cd "$dest" && pwd)
case "$dest" in "$engine"/*) echo "copy_project: the copy must be outside the engine folder" >&2; exit 2 ;; esac
rm -rf "$dest/build" "$dest/game" "$dest/small" "$dest/plain"
(cd "$engine/tests/consumer" && tar --exclude=./build --exclude=./engine_pin.cmake -cf - .) | (cd "$dest" && tar -xf -)
rm -f "$dest/engine_pin.cmake"
if [ "$mode" = pin ]; then
    pack=$(dirname "$dest")/nnhge-engine
    cmake -DENGINE="$engine" -DWORK="$pack" -P "$tools/pack_engine.cmake"
    cp "$pack/engine_pin.cmake" "$dest/engine_pin.cmake"
fi
echo "copy_project: $dest ($mode)"
