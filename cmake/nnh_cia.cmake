# nnh_add_3ds_package(<target> [ASSETS])  (spec 009, decision D22; called by nnh_add_game on 3DS)
#
# Distribution targets, none of them part of the normal build:
#   <target>_dist  copies <target>.3dsx (and builds <target>.cia, with CIA) into <build>/dist/<target>/
#   dist           every game's <target>_dist
#   <target>_cia   with nnh_add_game(... CIA ...): the installable .cia, made by makerom from the same ELF,
#                  SMDH and assets folder (as romfs), plus a banner made by bannertool.
# The .cia step reruns only when one of its inputs changes; the tools are fetched by its first build.

include("${CMAKE_CURRENT_LIST_DIR}/nnh_ctr_tools.cmake")

function(nnh_add_3ds_package target)
    set(_cmake_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}") # spec 032 FR-6: not the calling folder's
    cmake_parse_arguments(PARSE_ARGV 1 K "ASSETS" "" "")
    set(dist "${CMAKE_BINARY_DIR}/dist/${target}")
    set(threedsx "${CMAKE_CURRENT_BINARY_DIR}/${target}.3dsx")
    add_custom_target(${target}_dist
        COMMAND ${CMAKE_COMMAND} -E make_directory "${dist}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${threedsx}" "${dist}/"
        COMMENT "Distributable files of ${target} in dist/${target}"
        VERBATIM)
    add_dependencies(${target}_dist ${target}_3dsx)
    if(NOT TARGET dist)
        add_custom_target(dist)
    endif()
    add_dependencies(dist ${target}_dist)

    get_target_property(cia ${target} NNH_CIA)
    if(NOT cia)
        return()
    endif()
    nnh_ctr_tools()
    foreach(p TITLE VERSION_MAJOR VERSION_MINOR VERSION_MICRO CIA_UNIQUE_ID CIA_PRODUCT_CODE CIA_BANNER CIA_BANNER_AUDIO)
        get_target_property(${p} ${target} NNH_${p})
    endforeach()

    # Banner: checked, then made by bannertool.
    set(banner "${CMAKE_CURRENT_BINARY_DIR}/${target}.bnr")
    add_custom_command(OUTPUT "${banner}"
        COMMAND ${CMAKE_COMMAND} -DBANNER=${CIA_BANNER} -DAUDIO=${CIA_BANNER_AUDIO}
                -P ${_cmake_dir}/nnh_check_banner.cmake
        COMMAND "${NNH_BANNERTOOL_PROGRAM}" makebanner -i "${CIA_BANNER}" -a "${CIA_BANNER_AUDIO}" -o "${banner}"
        DEPENDS "${CIA_BANNER}" "${CIA_BANNER_AUDIO}" "${_cmake_dir}/nnh_check_banner.cmake"
        COMMENT "Banner for ${target}"
        VERBATIM)

    # romfs: the built assets folder, or an empty one for a game without assets.
    set(romfs_deps "")
    if(K_ASSETS)
        set(romfs "${CMAKE_CURRENT_BINARY_DIR}/assets")
        get_target_property(romfs_deps ${target}_assets DKP_ASSET_FILES)
    else()
        set(romfs "${CMAKE_CURRENT_BINARY_DIR}/${target}_empty_romfs")
        file(MAKE_DIRECTORY "${romfs}")
    endif()

    # The NCCH title holds at most 8 characters: the target's first letters and digits.
    string(REGEX REPLACE "[^A-Za-z0-9]" "" short "${target}")
    string(SUBSTRING "${short}" 0 8 short)

    set(cia_file "${dist}/${target}.cia")
    set(smdh "${CMAKE_CURRENT_BINARY_DIR}/${target}.smdh")
    add_custom_command(OUTPUT "${cia_file}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${dist}"
        COMMAND "${NNH_MAKEROM_PROGRAM}" -f cia -o "${cia_file}" -target t -exefslogo
                -elf "$<TARGET_FILE:${target}>" -rsf "${_cmake_dir}/3ds/nnhge.rsf"
                -icon "${smdh}" -banner "${banner}"
                -major ${VERSION_MAJOR} -minor ${VERSION_MINOR} -micro ${VERSION_MICRO}
                "-DAPP_TITLE=${short}" "-DAPP_PRODUCT_CODE=${CIA_PRODUCT_CODE}"
                "-DAPP_UNIQUE_ID=${CIA_UNIQUE_ID}" "-DAPP_ROMFS=${romfs}"
        DEPENDS ${target} "${smdh}" "${banner}" "${_cmake_dir}/3ds/nnhge.rsf" ${romfs_deps}
        COMMENT "CIA ${target}: unique ID ${CIA_UNIQUE_ID}, ${CIA_PRODUCT_CODE}, v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}"
        VERBATIM)
    add_custom_target(${target}_cia DEPENDS "${cia_file}")
    if(TARGET nnh_ctr_tools)
        add_dependencies(${target}_cia nnh_ctr_tools) # fetched before the banner and makerom steps
    endif()
    if(K_ASSETS)
        add_dependencies(${target}_cia ${target}_assets)
    endif()
    add_dependencies(${target}_dist ${target}_cia)
    message(STATUS "NNHGE: ${target}.cia with unique ID ${CIA_UNIQUE_ID} (${CIA_PRODUCT_CODE})")
endfunction()
