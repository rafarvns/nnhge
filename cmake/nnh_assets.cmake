# nnh_add_assets(<target> <folder>)
#
# The game's assets (spec 007; called by nnh_add_game(... ASSETS <folder>)). Every file under <folder>
# becomes an asset named by its path relative to it ('/'-separated). One build step per file, rerun only
# when that file changes, writes into <binary dir>/assets/<path>:
#   *.png  -> checked (nnh_check_image.cmake); 3DS: converted by tex3ds (same name), PC: copied;
#   *.wav, *.ogg -> checked (nnh_check_sound.cmake: PCM 8/16-bit or Vorbis, mono/stereo, 8-48 kHz), copied;
#   others -> copied unchanged.
# nnh_asset_index.cmake then writes <binary dir>/assets/.nnh_assets (the sorted list core reads at init),
# refuses invalid names and names differing only by letter case, and removes stale outputs.
# On PC that folder sits next to the executable; on 3DS it becomes the romfs of the .3dsx (the target
# `<target>_assets` is a devkitPro asset target, so ctr_create_3dsx(ROMFS) repacks when an asset changes).
#
# The folder is scanned with file(GLOB_RECURSE ... CONFIGURE_DEPENDS): an explicit exception to the
# "no GLOB" rule, because assets are data, not sources — adding a file must not require editing CMake.

set(_NNH_ASSETS_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(nnh_add_assets target folder)
    get_filename_component(source "${folder}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT IS_DIRECTORY "${source}")
        message(FATAL_ERROR "nnh_add_game(${target}): ASSETS folder not found: ${source}")
    endif()
    file(GLOB_RECURSE files LIST_DIRECTORIES false CONFIGURE_DEPENDS RELATIVE "${source}" "${source}/*")
    list(SORT files)
    set(out "${CMAKE_CURRENT_BINARY_DIR}/assets")

    set(outputs "")
    foreach(name IN LISTS files)
        set(input "${source}/${name}")
        set(output "${out}/${name}")
        get_filename_component(output_dir "${output}" DIRECTORY)
        if(name MATCHES "\\.png$")
            set(check ${CMAKE_COMMAND} -DIMAGE=${input} -P ${_NNH_ASSETS_DIR}/nnh_check_image.cmake)
            if(NNH_PLATFORM STREQUAL "3ds")
                if(NOT CTR_TEX3DS_EXE)
                    message(FATAL_ERROR "nnh_add_game(${target}): tex3ds not found (install it with devkitPro)")
                endif()
                set(convert ${CTR_TEX3DS_EXE} -f rgba8888 -z auto -o ${output} ${input})
            else()
                set(convert ${CMAKE_COMMAND} -E copy ${input} ${output})
            endif()
            add_custom_command(OUTPUT "${output}"
                COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
                COMMAND ${check}
                COMMAND ${convert}
                DEPENDS "${input}" "${_NNH_ASSETS_DIR}/nnh_check_image.cmake"
                COMMENT "Asset ${name}"
                VERBATIM)
        elseif(name MATCHES "\\.([wW][aA][vV]|[oO][gG][gG])$")
            add_custom_command(OUTPUT "${output}"
                COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
                COMMAND ${CMAKE_COMMAND} -DSOUND=${input} -P ${_NNH_ASSETS_DIR}/nnh_check_sound.cmake
                COMMAND ${CMAKE_COMMAND} -E copy ${input} ${output}
                DEPENDS "${input}" "${_NNH_ASSETS_DIR}/nnh_check_sound.cmake"
                COMMENT "Asset ${name}"
                VERBATIM)
        else()
            add_custom_command(OUTPUT "${output}"
                COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
                COMMAND ${CMAKE_COMMAND} -E copy ${input} ${output}
                DEPENDS "${input}"
                COMMENT "Asset ${name}"
                VERBATIM)
        endif()
        list(APPEND outputs "${output}")
    endforeach()

    # The list of names, rewritten only when it changes (so the index only reruns then).
    set(list_file "${CMAKE_CURRENT_BINARY_DIR}/nnh_assets_${target}.list")
    string(REPLACE ";" "\n" list_text "${files}")
    file(WRITE "${list_file}.new" "${list_text}\n")
    file(COPY_FILE "${list_file}.new" "${list_file}" ONLY_IF_DIFFERENT)

    set(index "${out}/.nnh_assets")
    add_custom_command(OUTPUT "${index}"
        COMMAND ${CMAKE_COMMAND} -DLIST=${list_file} -DOUT=${index} -DFOLDER=${out}
                -P ${_NNH_ASSETS_DIR}/nnh_asset_index.cmake
        DEPENDS "${list_file}" "${_NNH_ASSETS_DIR}/nnh_asset_index.cmake" ${outputs}
        COMMENT "Asset index for ${target}"
        VERBATIM)

    add_custom_target(${target}_assets DEPENDS ${outputs} "${index}")
    # A devkitPro asset target: ctr_create_3dsx(ROMFS) packs this folder and depends on these files.
    set_target_properties(${target}_assets PROPERTIES DKP_ASSET_FOLDER "${out}" DKP_ASSET_FILES "${outputs};${index}")
    add_dependencies(${target} ${target}_assets)
endfunction()
