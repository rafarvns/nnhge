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
#   *.gltf, *.glb (glTF 2.0 models, spec 026, D42) -> converted by nnh-assetc into <path without the extension>.nnhmesh;
#       a depfile lists the model, its buffers and its PNGs (packed as images, as usual); a model's buffers —
#       <model>.bin and <model>_<any>.bin beside a <model>.gltf — are not packed (plan 026 P25);
#   *.font (font descriptions, spec 029, D49) and *.fnt (BMFont) -> converted by nnh-assetc font into <path without the
#       extension>.nnhfont; a depfile lists the description, its source font, .chars files and the string files of
#       its `strings` languages (or the .fnt and its pages); *.ttf, *.otf, *.ttc, *.chars and a BMFont's pages
#       <font>_<any>.png beside its .fnt are not packed (the converter reads them); the licence a description names is
#       an ordinary asset;
#   strings/<code>.strings, strings/<code>/*.strings (spec 029, D50) -> not packed: per language `nnh-assetc strings`
#       writes strings/<code>.nnhstr and `nnh-assetc strings-index` the manifest .nnh_loc (nnh_strings.cmake);
#   others -> copied unchanged, except in a game with a data schema (spec 014, nnh_data.cmake): there every other
#       file goes through `nnh-assetc data`, which converts a file whose extension is a declared key
#       (`enemies/goblin.enemy`, same output name) and copies any other, writing a references sidecar in
#       <binary dir>/nnh_data/<target>/refs/; then `nnh-assetc data-index` checks every reference and writes the data
#       index `.nnh_data` (an asset of its own). Messages show paths relative to the folder of the calling
#       CMakeLists.txt (`assets/enemies/goblin.enemy:9: ...`). Both tools rewrite an output only when its bytes
#       change, so an edit that changes nothing converted repacks nothing.
# The .ase.json match comes before every generic rule, the data step included (D31), so it owns the suffix in every
# game.
# nnh_asset_index.cmake then writes <binary dir>/assets/.nnh_assets (the sorted list of output names core reads at
# init), refuses invalid names and names differing only by letter case, and removes stale outputs.
# nnh-assetc is only needed (and, in cross presets, only built) when the folder has a .tmj or a .ase.json, or the
# game a data schema (nnh_host_tools.cmake).
# On PC that folder sits next to the executable; on 3DS it becomes the romfs of the .3dsx (the target
# `<target>_assets` is a devkitPro asset target, so ctr_create_3dsx(ROMFS) repacks when an asset changes).
#
# The folder is scanned with file(GLOB_RECURSE ... CONFIGURE_DEPENDS): an explicit exception to the
# "no GLOB" rule, because assets are data, not sources — adding a file must not require editing CMake.

set(_NNH_ASSETS_DIR "${CMAKE_CURRENT_LIST_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/nnh_strings.cmake") # string files (spec 029)

# nnh_asset_output_name(<name> <out_var>): the asset name a source file becomes ("" = not packed).
function(nnh_asset_output_name name out_var)
    if(name MATCHES "\\.(tsj|tiled-project|tiled-session|ase|aseprite|ttf|otf|ttc|chars)$")
        set(${out_var} "" PARENT_SCOPE)
    elseif(name MATCHES "\\.(font|fnt)$") # spec 029: before the data rule (D29's routing, as D31)
        string(REGEX REPLACE "\\.(font|fnt)$" ".nnhfont" converted "${name}")
        set(${out_var} "${converted}" PARENT_SCOPE)
    elseif(name MATCHES "\\.ase\\.json$")
        string(REGEX REPLACE "\\.ase\\.json$" ".nnhsprite" converted "${name}")
        set(${out_var} "${converted}" PARENT_SCOPE)
    elseif(name MATCHES "\\.tmj$")
        string(REGEX REPLACE "\\.tmj$" ".nnhmap" converted "${name}")
        set(${out_var} "${converted}" PARENT_SCOPE)
    elseif(name MATCHES "\\.(gltf|glb)$")
        string(REGEX REPLACE "\\.(gltf|glb)$" ".nnhmesh" converted "${name}")
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

# One data step (spec 014): <source>/<name> -> <out>/<name> (converted or copied) + its references sidecar.
function(_nnh_add_data_step source out name assetc assetc_depends schema refs)
    set(input "${source}/${name}")
    set(output "${out}/${name}")
    get_filename_component(output_dir "${output}" DIRECTORY)
    get_filename_component(refs_dir "${refs}" DIRECTORY)
    file(RELATIVE_PATH shown "${CMAKE_CURRENT_SOURCE_DIR}" "${input}")
    add_custom_command(OUTPUT "${output}" "${refs}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir} ${refs_dir}
        COMMAND ${assetc} data ${input} ${output} --schema ${schema} --refs ${refs} --name ${name} --show ${shown}
        DEPENDS "${input}" "${schema}" ${assetc_depends}
        COMMENT "Data ${name}"
        VERBATIM)
endfunction()

# The data index step (spec 014): every sidecar + the asset list -> <out>/.nnh_data.
function(_nnh_add_data_index target source out list_file sidecars assetc assetc_depends schema)
    set(refs_list "${CMAKE_CURRENT_BINARY_DIR}/nnh_data/${target}/refs.list")
    # Relative to the list's folder: in cross presets this file is read by a native program, which can't open the
    # MSYS paths devkitPro's CMake writes.
    set(relative "")
    foreach(sidecar IN LISTS sidecars)
        file(RELATIVE_PATH r "${CMAKE_CURRENT_BINARY_DIR}/nnh_data/${target}" "${sidecar}")
        list(APPEND relative "${r}")
    endforeach()
    string(REPLACE ";" "\n" text "${relative}")
    file(WRITE "${refs_list}.new" "${text}\n")
    file(COPY_FILE "${refs_list}.new" "${refs_list}" ONLY_IF_DIFFERENT)
    file(RELATIVE_PATH shown "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
    add_custom_command(OUTPUT "${out}/.nnh_data"
        COMMAND ${assetc} data-index --out ${out}/.nnh_data --schema ${schema} --assets-list ${list_file}
                --refs-list ${refs_list} --show ${shown}
        DEPENDS ${sidecars} "${list_file}" "${refs_list}" "${schema}" ${assetc_depends}
        COMMENT "Data index for ${target}"
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

# One model conversion step (spec 026): <source>/<name> (.gltf/.glb) -> <out>/<output name> (.nnhmesh), with a
# depfile listing the model, its buffers and its textures.
function(_nnh_add_mesh_step target source out name output_name assetc assetc_depends)
    set(output "${out}/${output_name}")
    set(depfile "${CMAKE_CURRENT_BINARY_DIR}/nnh_meshes/${output_name}.d")
    get_filename_component(output_dir "${output}" DIRECTORY)
    get_filename_component(depfile_dir "${depfile}" DIRECTORY)
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir} ${depfile_dir}
        COMMAND ${assetc} mesh ${source}/${name} ${output} --assets ${source} --depfile ${depfile}
        DEPENDS "${source}/${name}" ${assetc_depends}
        DEPFILE "${depfile}"
        COMMENT "Model ${name}"
        VERBATIM)
endfunction()

# One font conversion step (spec 029): <source>/<name> (.font or .fnt) -> <out>/<output name> (.nnhfont), with a
# depfile; every string file of the game is passed (`--strings code=file`): the converter reads those of the languages
# the description lists.
function(_nnh_add_font_step target source out name output_name assetc assetc_depends)
    set(output "${out}/${output_name}")
    set(depfile "${CMAKE_CURRENT_BINARY_DIR}/nnh_fonts/${output_name}.d")
    get_filename_component(output_dir "${output}" DIRECTORY)
    get_filename_component(depfile_dir "${depfile}" DIRECTORY)
    string(REGEX REPLACE "\\.(font|fnt)$" "" shown "${name}")
    get_target_property(strings ${target} NNH_STRING_SOURCES) # "code=file" entries (nnh_strings.cmake)
    set(strings_args "")
    if(strings)
        foreach(entry IN LISTS strings)
            list(APPEND strings_args --strings "${entry}")
        endforeach()
    endif()
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir} ${depfile_dir}
        COMMAND ${assetc} font ${source}/${name} ${output} --assets ${source} --show ${shown} --depfile ${depfile}
                ${strings_args}
        DEPENDS "${source}/${name}" ${assetc_depends}
        DEPFILE "${depfile}"
        COMMENT "Font ${name}"
        VERBATIM)
endfunction()

# _nnh_bmfont_pages(<files> <out_var>): the PNG pages that belong to a .fnt beside them (<font>_<any>.png).
function(_nnh_bmfont_pages files out_var)
    set(pages "")
    foreach(name IN LISTS files)
        if(NOT name MATCHES "^(.*)\\.fnt$")
            continue()
        endif()
        set(stem "${CMAKE_MATCH_1}")
        string(LENGTH "${stem}" n)
        foreach(other IN LISTS files)
            string(SUBSTRING "${other}" 0 ${n} head)
            if(head STREQUAL stem)
                string(SUBSTRING "${other}" ${n} -1 rest)
                if(rest MATCHES "^_[^/]+\\.png$")
                    list(APPEND pages "${other}")
                endif()
            endif()
        endforeach()
    endforeach()
    set(${out_var} "${pages}" PARENT_SCOPE)
endfunction()

# _nnh_model_buffers(<files> <out_var>): the .bin files that belong to a .gltf beside them (plan 026 P25).
function(_nnh_model_buffers files out_var)
    set(buffers "")
    foreach(name IN LISTS files)
        if(NOT name MATCHES "^(.*)\\.gltf$")
            continue()
        endif()
        set(stem "${CMAKE_MATCH_1}")
        string(LENGTH "${stem}" n)
        foreach(other IN LISTS files)
            string(SUBSTRING "${other}" 0 ${n} head)
            if(head STREQUAL stem)
                string(SUBSTRING "${other}" ${n} -1 rest) # ".bin" or "_<any>.bin" (no folder)
                if(rest STREQUAL ".bin" OR rest MATCHES "^_[^/]+\\.bin$")
                    list(APPEND buffers "${other}")
                endif()
            endif()
        endforeach()
    endforeach()
    set(${out_var} "${buffers}" PARENT_SCOPE)
endfunction()

function(nnh_add_assets target folder)
    get_filename_component(source "${folder}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT IS_DIRECTORY "${source}")
        message(FATAL_ERROR "nnh_add_game(${target}): ASSETS folder not found: ${source}")
    endif()
    file(GLOB_RECURSE files LIST_DIRECTORIES false CONFIGURE_DEPENDS RELATIVE "${source}" "${source}/*")
    list(SORT files)
    set(out "${CMAKE_CURRENT_BINARY_DIR}/assets")
    get_target_property(schema ${target} NNH_DATA_SCHEMA_FILE) # set by nnh_add_data_schema (DATA_SCHEMA)
    if(NOT schema)
        set(schema "")
    endif()
    if(schema)
        nnh_assetc_command(assetc assetc_depends "data assets")
        if(".nnh_data" IN_LIST files)
            message(FATAL_ERROR "nnh_add_game(${target}): the asset name .nnh_data is reserved (the data index)")
        endif()
        set(refs_dir "${CMAKE_CURRENT_BINARY_DIR}/nnh_data/${target}/refs")
        set(sidecars "")
    elseif(files MATCHES "\\.tmj(;|$)")
        nnh_assetc_command(assetc assetc_depends "Tiled maps") # only games with maps, sprites or data build it
    elseif(files MATCHES "\.ase\.json(;|$)")
        nnh_assetc_command(assetc assetc_depends "Aseprite exports (.ase.json)")
    elseif(files MATCHES "\\.(gltf|glb)(;|$)")
        nnh_assetc_command(assetc assetc_depends "glTF models (.gltf, .glb)")
    elseif(files MATCHES "\\.(font|fnt)(;|$)")
        nnh_assetc_command(assetc assetc_depends "fonts (.font, .fnt)")
    endif()
    _nnh_strings_files(${target} "${files}" "${source}" string_files) # spec 029: checked against LANGUAGES
    if(string_files AND NOT assetc)
        nnh_assetc_command(assetc assetc_depends "string files (.strings)")
    endif()
    _nnh_model_buffers("${files}" model_buffers)
    _nnh_bmfont_pages("${files}" font_pages)
    list(APPEND model_buffers ${font_pages} ${string_files}) # not packed either
    set(font_outputs "")

    set(outputs "")
    set(names "") # output names, for the index
    foreach(name IN LISTS files)
        nnh_asset_output_name("${name}" output_name)
        if(output_name STREQUAL "" OR name IN_LIST model_buffers)
            continue()
        endif()
        if(output_name IN_LIST names)
            message(FATAL_ERROR "nnh_add_game(${target}): two files would both become the asset ${output_name}: "
                                "remove one of them")
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
        elseif(name MATCHES "\\.(gltf|glb)$")
            _nnh_add_mesh_step(${target} "${source}" "${out}" "${name}" "${output_name}" "${assetc}" "${assetc_depends}")
        elseif(name MATCHES "\\.(font|fnt)$")
            _nnh_add_font_step(${target} "${source}" "${out}" "${name}" "${output_name}" "${assetc}" "${assetc_depends}")
            list(APPEND font_outputs "${output}")
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
        elseif(schema AND NOT name MATCHES "\\.(ase\\.json|ase|aseprite)$")
            set(refs "${refs_dir}/${name}.refs")
            _nnh_add_data_step("${source}" "${out}" "${name}" "${assetc}" "${assetc_depends}" "${schema}" "${refs}")
            list(APPEND sidecars "${refs}")
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

    if(string_files)
        _nnh_strings_steps(${target} "${source}" "${out}" "${assetc}" "${assetc_depends}" "${font_outputs}" outputs names)
    endif()
    set(generated "")
    if(schema)
        list(APPEND names ".nnh_data")
        set(generated -DGENERATED=.nnh_data)
    endif()
    # The list of names, rewritten only when it changes (so the index only reruns then).
    set(list_file "${CMAKE_CURRENT_BINARY_DIR}/nnh_assets_${target}.list")
    string(REPLACE ";" "\n" list_text "${names}")
    file(WRITE "${list_file}.new" "${list_text}\n")
    file(COPY_FILE "${list_file}.new" "${list_file}" ONLY_IF_DIFFERENT)
    if(schema)
        _nnh_add_data_index(${target} "${source}" "${out}" "${list_file}" "${sidecars}" "${assetc}"
                            "${assetc_depends}" "${schema}")
        list(APPEND outputs "${out}/.nnh_data")
    endif()

    set(index "${out}/.nnh_assets")
    add_custom_command(OUTPUT "${index}"
        COMMAND ${CMAKE_COMMAND} -DLIST=${list_file} -DOUT=${index} -DFOLDER=${out} ${generated}
                -P ${_NNH_ASSETS_DIR}/nnh_asset_index.cmake
        DEPENDS "${list_file}" "${_NNH_ASSETS_DIR}/nnh_asset_index.cmake" ${outputs}
        COMMENT "Asset index for ${target}"
        VERBATIM)

    add_custom_target(${target}_assets DEPENDS ${outputs} "${index}")
    # A devkitPro asset target: ctr_create_3dsx(ROMFS) packs this folder and depends on these files.
    set_target_properties(${target}_assets PROPERTIES DKP_ASSET_FOLDER "${out}" DKP_ASSET_FILES "${outputs};${index}")
    add_dependencies(${target} ${target}_assets)
    if(schema)
        get_target_property(schema_target ${target} NNH_DATA_SCHEMA_TARGET)
        add_dependencies(${target}_assets ${schema_target})
    endif()
endfunction()
