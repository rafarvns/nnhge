# nnh_add_assets(<target> <folder>)
#
# The game's assets (spec 007; called by nnh_add_game(... ASSETS <folder>)). Every file under <folder>
# becomes an asset named by its path relative to it ('/'-separated). One build step per file, rerun only
# when that file changes, writes into <binary dir>/assets/<path>:
#   *.png  -> checked (nnh_check_image.cmake); 3DS: converted by tex3ds (same name), PC: copied;
#   *.wav, *.ogg -> checked (nnh_check_sound.cmake: PCM 8/16-bit or Vorbis, mono/stereo, 8-48 kHz), copied;
#   *.tmj (Tiled maps, spec 012) -> converted by nnh-assetc into <path without .tmj>.nnhmap; a Ninja depfile
#       lists the tilesets and images it read, so editing a shared .tsj reconverts exactly the maps using it;
#   *.tsj, *.tiled-project, *.tiled-session -> not packed (Tiled's own files; the converter reads the .tsj);
#   *.ase.json (Aseprite sprite sheet data, spec 016, D31) -> converted by nnh-assetc into <path without .ase.json>
#       .nnhsprite; a depfile lists the export and the PNG beside it (that PNG is packed as an image, as usual);
#   *.ase, *.aseprite -> not packed (Aseprite's own files, if kept in the folder);
#   others -> copied unchanged.
# The .ase.json match comes before every generic rule (D31), so it owns the suffix in every game.
# nnh_asset_index.cmake then writes <binary dir>/assets/.nnh_assets (the sorted list of output names core reads at
# init), refuses invalid names and names differing only by letter case, and removes stale outputs.
# nnh-assetc is only needed (and, in cross presets, only built) when the folder has a .tmj or a .ase.json
# (nnh_host_tools.cmake).
# On PC that folder sits next to the executable; on 3DS it becomes the romfs of the .3dsx (the target
# `<target>_assets` is a devkitPro asset target, so ctr_create_3dsx(ROMFS) repacks when an asset changes).
#
# The folder is scanned with file(GLOB_RECURSE ... CONFIGURE_DEPENDS): an explicit exception to the
# "no GLOB" rule, because assets are data, not sources — adding a file must not require editing CMake.

set(_NNH_ASSETS_DIR "${CMAKE_CURRENT_LIST_DIR}")

# nnh_asset_output_name(<name> <out_var>): the asset name a source file becomes ("" = not packed).
function(nnh_asset_output_name name out_var)
    if(name MATCHES "\\.(tsj|tiled-project|tiled-session|ase|aseprite)$")
        set(${out_var} "" PARENT_SCOPE)
    elseif(name MATCHES "\\.ase\\.json$")
        string(REGEX REPLACE "\\.ase\\.json$" ".nnhsprite" converted "${name}")
        set(${out_var} "${converted}" PARENT_SCOPE)
    elseif(name MATCHES "\\.tmj$")
        string(REGEX REPLACE "\\.tmj$" ".nnhmap" converted "${name}")
        set(${out_var} "${converted}" PARENT_SCOPE)
    else()
        set(${out_var} "${name}" PARENT_SCOPE)
    endif()
endfunction()

# One map conversion step: <source>/<name> (.tmj) -> <out>/<output name>, with a depfile beside the target's
# other build files (not in the packed folder).
function(_nnh_add_map_step target source out name output_name assetc assetc_depends)
    set(output "${out}/${output_name}")
    set(depfile "${CMAKE_CURRENT_BINARY_DIR}/nnh_maps/${output_name}.d")
    get_filename_component(output_dir "${output}" DIRECTORY)
    get_filename_component(depfile_dir "${depfile}" DIRECTORY)
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir} ${depfile_dir}
        COMMAND ${assetc} map ${source}/${name} ${output} --assets ${source} --depfile ${depfile}
        DEPENDS "${source}/${name}" ${assetc_depends}
        DEPFILE "${depfile}"
        COMMENT "Map ${name}"
        VERBATIM)
endfunction()

# One sprite conversion step (spec 016): <source>/<name> (.ase.json) -> <out>/<output name> (.nnhsprite), with a
# depfile listing the export and its PNG.
function(_nnh_add_sprite_step target source out name output_name assetc assetc_depends)
    set(output "${out}/${output_name}")
    set(depfile "${CMAKE_CURRENT_BINARY_DIR}/nnh_sprites/${output_name}.d")
    get_filename_component(output_dir "${output}" DIRECTORY)
    get_filename_component(depfile_dir "${depfile}" DIRECTORY)
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir} ${depfile_dir}
        COMMAND ${assetc} sprite ${source}/${name} ${output} --assets ${source} --depfile ${depfile}
        DEPENDS "${source}/${name}" ${assetc_depends}
        DEPFILE "${depfile}"
        COMMENT "Sprite ${name}"
        VERBATIM)
endfunction()

function(nnh_add_assets target folder)
    get_filename_component(source "${folder}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT IS_DIRECTORY "${source}")
        message(FATAL_ERROR "nnh_add_game(${target}): ASSETS folder not found: ${source}")
    endif()
    file(GLOB_RECURSE files LIST_DIRECTORIES false CONFIGURE_DEPENDS RELATIVE "${source}" "${source}/*")
    list(SORT files)
    set(out "${CMAKE_CURRENT_BINARY_DIR}/assets")
    # Only games with maps or Aseprite exports need (and build) the converter.
    if(files MATCHES "\\.tmj(;|$)")
        nnh_assetc_command(assetc assetc_depends "Tiled maps")
    elseif(files MATCHES "\\.ase\\.json(;|$)")
        nnh_assetc_command(assetc assetc_depends "Aseprite exports (.ase.json)")
    endif()

    set(outputs "")
    set(names "") # output names, for the index
    foreach(name IN LISTS files)
        nnh_asset_output_name("${name}" output_name)
        if(output_name STREQUAL "")
            continue()
        endif()
        if(NOT output_name STREQUAL name AND output_name IN_LIST files)
            message(FATAL_ERROR "nnh_add_game(${target}): ${name} and ${output_name} would both become the asset "
                                "${output_name}: remove one of them")
        endif()
        list(APPEND names "${output_name}")
        set(input "${source}/${name}")
        set(output "${out}/${output_name}")
        get_filename_component(output_dir "${output}" DIRECTORY)
        if(name MATCHES "\\.tmj$")
            _nnh_add_map_step(${target} "${source}" "${out}" "${name}" "${output_name}" "${assetc}" "${assetc_depends}")
        elseif(name MATCHES "\\.ase\\.json$")
            _nnh_add_sprite_step(${target} "${source}" "${out}" "${name}" "${output_name}" "${assetc}"
                                 "${assetc_depends}")
        elseif(name MATCHES "\\.png$")
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
    string(REPLACE ";" "\n" list_text "${names}")
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
