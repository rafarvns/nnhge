#!/bin/sh
# Linux checks on the homelab build VM (ssh nnhge-build, VM 120; start it first, see CLAUDE.md "Build").
#
#   scripts/vm_check.sh sync              send the working tree (incremental: build folders on the VM are kept)
#   scripts/vm_check.sh fast              sync + intermediate pass (unit tests on GCC 13 and under ASan/UBSan,
#                                         FUZZ targets 2 min each)
#   scripts/vm_check.sh final             sync + final pass (full ctest on GCC 13 with the consumer-full ways, the
#                                         consumer project's 3DS builds through nnh from a copy and a fresh 3ds-dev
#                                         `nnh build animation`, the 3DS command-length check on both (spec 032), a project made by
#                                         `nnh new` (doctor without/with DEVKITPRO, 3DS builds, smoke script; spec 033), unit tests on GCC 14 and the 32-bit host,
#                                         ASan/UBSan, FUZZ targets 10 min each)
#   FUZZ="tests/fuzz_remote tools/assetc/fuzz_assetc_ttf" scripts/vm_check.sh fast   # fuzz targets, relative to build/asan
#   CLEAN=1 scripts/vm_check.sh final     wipe the VM's tree and build folders first
#
# The tree is sent with each file's own modification time, so Ninja and ccache rebuild only what changed; a file
# whose content changed since the last sync is touched on the VM, so an object built there after the file's local
# edit (a build that ran while the edit was being made) is still rebuilt. Files deleted locally are deleted on the
# VM too (build/ is never touched by the sync).
set -eu
mode="${1:-fast}"
host=nnhge-build
cd "$(git rev-parse --show-toplevel)"

sync_tree() {
    if [ "${CLEAN:-0}" = 1 ]; then ssh "$host" 'rm -rf ~/wt/nnhge'; fi
    git ls-files -co --exclude-standard > .vm_files
    tr '\n' '\0' < .vm_files | tar --null -T - -cf - .vm_files | ssh "$host" '
        set -e; mkdir -p ~/wt/nnhge; cd ~/wt/nnhge
        find . -path ./build -prune -o -type f -print0 | xargs -0 -r md5sum > /tmp/vm_sums 2>/dev/null || true
        tar -xf -
        md5sum --quiet -c /tmp/vm_sums 2>/dev/null | sed -n "s/: FAILED\$//p" | while IFS= read -r f; do touch -- "$f"; done
        find . -path ./build -prune -o -type f -print | sed "s|^\./||" | grep -v "^\.vm_files$" | sort > /tmp/vm_have
        sort .vm_files > /tmp/vm_want
        comm -23 /tmp/vm_have /tmp/vm_want | while IFS= read -r f; do rm -f -- "$f"; done'
    rm -f .vm_files
    echo "vm_check: tree synced"
}

remote() { ssh "$host" "cd ~/wt/nnhge && $1"; }

UNIT='cmake --preset pc-debug >/dev/null && cmake --build --preset pc-debug --target nnh_tests nnh_tool_tests nnh_assetc_tests 2>&1 | grep -E "warning:|error:|FAILED" | head -20; build/pc-debug/tests/nnh_tests | tail -1; find build/pc-debug -name nnh_tool_tests -type f -perm -u+x -exec {} \; | tail -1; find build/pc-debug -name nnh_assetc_tests -type f -perm -u+x -exec {} \; | tail -1'
ASAN='F="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"; CC=clang-18 CXX=clang++-18 cmake --preset pc-debug -B build/asan -DNNH_BUILD_FUZZERS=ON -DCMAKE_C_FLAGS="$F" -DCMAKE_CXX_FLAGS="$F" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" >/dev/null && cmake --build build/asan 2>&1 | grep -E "warning:|error:|FAILED" | head -20; for t in tests/nnh_tests $(cd build/asan && find . -name nnh_tool_tests -o -name nnh_assetc_tests | grep -v CMakeFiles); do echo "asan $t: $(ASAN_OPTIONS=detect_leaks=1 build/asan/$t 2>&1 | grep -E "ERROR: |runtime error|^FAIL|tests, " | head -5 | tr "\n" " ")"; done'

fuzz() {
    [ -n "${FUZZ:-}" ] || return 0
    remote "cd build/asan && for t in $FUZZ; do n=\$(basename \$t); mkdir -p /tmp/corp/\$n; (ASAN_OPTIONS=detect_leaks=1 timeout $(($1 + 60)) ./\$t -max_total_time=$1 -rss_limit_mb=1500 /tmp/corp/\$n > /tmp/fuzz_\$n.log 2>&1; echo \"fuzz \$n exit \$? \$(grep -E 'Done [0-9]+ runs' /tmp/fuzz_\$n.log | tail -1) \$(grep -m1 -E 'ERROR|runtime error|deadly|crash-' /tmp/fuzz_\$n.log)\") & done; wait"
}

case "$mode" in
sync) sync_tree ;;
fast)
    sync_tree
    remote "$UNIT"
    remote "$ASAN"
    fuzz 120
    ;;
final)
    sync_tree
    remote 'cmake --preset pc-debug >/dev/null && cmake --build --preset pc-debug 2>&1 | grep -E "warning:|error:|FAILED" | head -20; ctest --preset pc-debug -j4 -LE consumer-full 2>&1 | tail -6'
    # Spec 032: the consumer game project in every way, and its 3DS builds through nnh from a copy outside the tree.
    remote 'ctest --preset pc-debug -L consumer-full 2>&1 | tail -4'
    remote 'export DEVKITPRO=/opt/devkitpro E=$PWD N=$PWD/build/pc-debug/tools/nnh/nnh C=/tmp/nnh-consumer/a-game-project-folder-with-a-deliberately-long-name; rm -rf /tmp/nnh-consumer && tests/consumer_tools/copy_project.sh $C pin >/dev/null && (cd $C && $N build nnh_consumer 2>&1 | tail -1 && $N build nnh_consumer --release 2>&1 | tail -1 && ls build/3ds-*/game/nnh_consumer.3dsx && for p in 3ds-dev 3ds-release; do $E/scripts/check_command_length.sh build/$p | head -1; done); rm -rf /tmp/nnh-consumer; rm -rf build/3ds-dev && $N build animation 2>&1 | tail -1 && scripts/check_command_length.sh build/3ds-dev | head -1'
    # Spec 033: a project made by nnh new under /tmp, as "Your first game" walks it on Linux: nnh doctor without and
    # with DEVKITPRO (exit 1 with the FAIL and its fix, then exit 0), its 3DS builds through nnh with /opt/devkitpro
    # (+ the .cia), the command-length check, the simulator build and its smoke script headless.
    remote 'E=$PWD N=$PWD/build/pc-debug/tools/nnh/nnh G=/tmp/nnh-new; rm -rf $G && mkdir -p $G && cd $G && $N new demo --engine $E | head -1 && cd demo && export NNHGE_ENGINE_DIR=$E && (unset DEVKITPRO; $N doctor > /tmp/doctor_unset.txt; echo "doctor without DEVKITPRO: exit $?"; grep -A1 "FAIL  devkitPro" /tmp/doctor_unset.txt; tail -7 /tmp/doctor_unset.txt) && export DEVKITPRO=/opt/devkitpro && { $N doctor > /tmp/doctor_set.txt; echo "doctor with DEVKITPRO: exit $?"; cat /tmp/doctor_set.txt; } && $N build demo 2>&1 | tail -1 && $N build demo --release 2>&1 | tail -1 && cmake --build build/3ds-dev --target dist 2>&1 | tail -1 && ls build/3ds-dev/dist/demo/ && for p in 3ds-dev 3ds-release; do $E/scripts/check_command_length.sh build/$p | head -1; done; SDL_VIDEO_DRIVER=offscreen SDL_RENDER_DRIVER=software NNH_SIM_RENDERER=sdl SDL_AUDIO_DRIVER=dummy $N run demo --pc --script tests/smoke.nnhs 2>&1 | grep -E "nnh: (PASS|FAIL)"; cd / && rm -rf $G'
    remote 'CC=gcc-14 CXX=g++-14 cmake --preset pc-debug -B build/gcc14 >/dev/null && cmake --build build/gcc14 --target nnh_tests 2>&1 | grep -E "warning:|error:" | head; echo "gcc14: $(build/gcc14/tests/nnh_tests | tail -1)"'
    remote 'F="-m32 -msse2 -mfpmath=sse"; cmake --preset pc-debug -B build/m32 -DCMAKE_C_FLAGS="$F" -DCMAKE_CXX_FLAGS="$F" -DCMAKE_EXE_LINKER_FLAGS=-m32 >/dev/null 2>&1 && cmake --build build/m32 --target nnh_tests 2>&1 | grep -E "error:" | head -3; echo "m32: $(build/m32/tests/nnh_tests | tail -1)"'
    remote "$ASAN"
    fuzz 600
    ;;
*) echo "usage: scripts/vm_check.sh sync|fast|final" >&2; exit 2 ;;
esac
