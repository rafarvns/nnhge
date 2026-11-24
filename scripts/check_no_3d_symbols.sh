#!/bin/sh
# A 2D game links no 3D code (spec 026 FR-31, AC-9): lists the 3D symbols of a built program (3DS .elf with devkitARM's
# nm, PC .exe with the host's nm). Allowed: the hook pointer nnh::detail::g_gfx_3d_ops. Exit 1 if anything else shows.
#
#   scripts/check_no_3d_symbols.sh build/3ds-release/examples/hello/hello.elf
#   scripts/check_no_3d_symbols.sh build/pc-release/examples/hello/hello.exe

program=$1
case "$program" in
*.elf) nm=${DEVKITARM:-/c/devkitPro/devkitARM}/bin/arm-none-eabi-nm ;;
*) nm=nm ;;
esac
found=$("$nm" -C "$program" | grep -E "gfx3d|video3d|raster3d|mesh_|Mesh|math3d|Quat|Mat4|Vec3|begin_3d|draw_3d|end_3d|mesh3d_shader|C3D_Light|mesh3d" |
    grep -v "g_gfx_3d_ops")
if [ -n "$found" ]; then
    echo "$found"
    echo "3d symbols: FOUND in $program"
    exit 1
fi
echo "3d symbols: none in $program (the hook pointer only)"
