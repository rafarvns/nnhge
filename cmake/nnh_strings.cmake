# A game's string files (spec 029 FR-15, FR-20, D50; included by nnh_assets.cmake).
#
# nnh_add_game(... ASSETS <folder> LANGUAGES en pt-BR ja [BASE en] [STRICT_STRINGS]) — each language's strings are
# <folder>/strings/<code>.strings or every file of <folder>/strings/<code>/ (*.strings). Per language one step,
# `nnh-assetc strings <code>` → strings/<code>.nnhstr (it reads its own files and the base's, so editing de.strings
# reruns only the de step, the fonts listing de, and the index); then one `nnh-assetc strings-index` → .nnh_loc (and,
# in Debug/Development, the pseudo-language strings/qps.nnhstr). Release builds pass --release (no key names, no qps);
# STRICT_STRINGS turns warnings into errors. The language list (code, files; the base marked) is a generated file
# rewritten only when it changes.

# _nnh_strings_files(<target> <files> <source> <out_files_var>): the string files, checked against LANGUAGES/BASE;
# sets the target's NNH_STRING_SOURCES ("code=file") and NNH_STRINGS_LIST (the list file).
function(_nnh_strings_files target files source out_var)
    get_target_property(languages ${target} NNH_LANGUAGES)
    get_target_property(base ${target} NNH_BASE_LANGUAGE)
    set(strings "")
    foreach(name IN LISTS files)
        if(name MATCHES "\\.strings$")
            if(NOT name MATCHES "^strings/")
                message(FATAL_ERROR "nnh_add_game(${target}): ${name}: string files live in strings/ "
                                    "(strings/<code>.strings or strings/<code>/*.strings)")
            endif()
            list(APPEND strings "${name}")
        endif()
    endforeach()
    if(NOT languages)
        if(strings)
            message(FATAL_ERROR "nnh_add_game(${target}): string files but no LANGUAGES (e.g. LANGUAGES en pt-BR ja)")
        endif()
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    if(NOT base)
        list(GET languages 0 base)
    endif()
    if(NOT base IN_LIST languages)
        message(FATAL_ERROR "nnh_add_game(${target}): BASE ${base} is not in LANGUAGES")
    endif()
    set(list_text "")
    set(sources "")
    foreach(code IN LISTS languages)
        if(NOT code MATCHES "^[a-z][a-z][a-z]?(-[A-Z][a-z][a-z][a-z])?(-([A-Z][A-Z]|[0-9][0-9][0-9]))?$")
            message(FATAL_ERROR "nnh_add_game(${target}): \"${code}\" is not a language code (en, pt-BR, zh-Hant, es-419)")
        endif()
        set(own "")
        foreach(name IN LISTS strings)
            if(name STREQUAL "strings/${code}.strings" OR name MATCHES "^strings/${code}/[^/]+\\.strings$")
                list(APPEND own "${source}/${name}")
            endif()
        endforeach()
        if(NOT own)
            message(FATAL_ERROR "nnh_add_game(${target}): language ${code} has no strings/${code}.strings "
                                "(or strings/${code}/*.strings)")
        endif()
        if("strings/${code}.strings" IN_LIST strings AND own MATCHES "strings/${code}/")
            message(FATAL_ERROR "nnh_add_game(${target}): ${code} has both strings/${code}.strings and strings/${code}/: "
                                "keep one form")
        endif()
        foreach(file IN LISTS own)
            list(APPEND sources "${code}=${file}")
        endforeach()
        # relative to the list's folder: in cross presets the list is read by a native program, which can't open the
        # MSYS paths devkitPro's CMake writes (as the data index's refs list)
        set(relative "")
        foreach(file IN LISTS own)
            file(RELATIVE_PATH r "${CMAKE_CURRENT_BINARY_DIR}" "${file}")
            list(APPEND relative "${r}")
        endforeach()
        list(JOIN relative ";" joined)
        if(code STREQUAL base)
            string(APPEND list_text "*")
        endif()
        string(APPEND list_text "${code}\t${joined}\n")
    endforeach()
    foreach(name IN LISTS strings) # files of languages not listed
        string(REGEX REPLACE "^strings/([^/.]+)(\\.strings|/.*)$" "\\1" code "${name}")
        if(NOT code IN_LIST languages)
            message(FATAL_ERROR "nnh_add_game(${target}): ${name} belongs to ${code}, which is not in LANGUAGES")
        endif()
    endforeach()
    set(list_file "${CMAKE_CURRENT_BINARY_DIR}/nnh_strings_${target}.list")
    file(WRITE "${list_file}.new" "${list_text}")
    file(COPY_FILE "${list_file}.new" "${list_file}" ONLY_IF_DIFFERENT)
    set_target_properties(${target} PROPERTIES NNH_STRING_SOURCES "${sources}" NNH_STRINGS_LIST "${list_file}"
                                               NNH_BASE_LANGUAGE "${base}")
    set(${out_var} "${strings}" PARENT_SCOPE)
endfunction()

# _nnh_strings_steps(<target> <source> <out> <assetc> <assetc_depends> <fonts> <outputs_var> <names_var>): the per-language
# steps and the index step; appends their outputs and asset names.
function(_nnh_strings_steps target source out assetc assetc_depends fonts outputs_var names_var)
    get_target_property(languages ${target} NNH_LANGUAGES)
    get_target_property(list_file ${target} NNH_STRINGS_LIST)
    get_target_property(sources ${target} NNH_STRING_SOURCES)
    get_target_property(strict ${target} NNH_STRICT_STRINGS)
    set(outputs ${${outputs_var}})
    set(names ${${names_var}})
    set(flags "")
    if(CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
        set(flags --release)
    endif()
    if(strict)
        list(APPEND flags --strict)
    endif()
    set(all_files "")
    foreach(entry IN LISTS sources)
        string(REGEX REPLACE "^[^=]+=" "" file "${entry}")
        list(APPEND all_files "${file}")
    endforeach()
    get_target_property(base ${target} NNH_BASE_LANGUAGE)
    set(tables "")
    foreach(code IN LISTS languages)
        set(output "${out}/strings/${code}.nnhstr")
        # a language's table reads its own files and the base's: editing de.strings reruns only de's step (and the index)
        set(step_files "")
        foreach(entry IN LISTS sources)
            if(entry MATCHES "^${code}=" OR entry MATCHES "^${base}=")
                string(REGEX REPLACE "^[^=]+=" "" file "${entry}")
                list(APPEND step_files "${file}")
            endif()
        endforeach()
        add_custom_command(OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory ${out}/strings
            COMMAND ${assetc} strings ${code} --languages ${list_file} --out ${output} ${flags}
            DEPENDS ${step_files} "${list_file}" ${assetc_depends}
            COMMENT "Strings ${code}"
            VERBATIM)
        list(APPEND tables "${output}")
        list(APPEND names "strings/${code}.nnhstr")
    endforeach()
    set(index "${out}/.nnh_loc")
    set(index_outputs "${index}")
    set(pseudo "")
    if(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
        set(pseudo --pseudo-out ${out}/strings/qps.nnhstr)
        list(APPEND index_outputs "${out}/strings/qps.nnhstr")
        list(APPEND names "strings/qps.nnhstr")
    endif()
    add_custom_command(OUTPUT ${index_outputs}
        COMMAND ${assetc} strings-index --languages ${list_file} --out ${index} --fonts ${out} ${pseudo} ${flags}
        DEPENDS ${tables} ${fonts} ${all_files} "${list_file}" ${assetc_depends}
        COMMENT "Languages of ${target}"
        VERBATIM)
    list(APPEND names ".nnh_loc")
    list(APPEND outputs ${tables} ${index_outputs})
    set(${outputs_var} "${outputs}" PARENT_SCOPE)
    set(${names_var} "${names}" PARENT_SCOPE)
endfunction()
