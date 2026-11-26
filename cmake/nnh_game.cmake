# nnh_add_game(<target>
#     SOURCES <files...>
#     [ASSETS <folder>]
#     [IMAGES <png files...>]
#     [DATA_SCHEMA <schema sources...>]
#     [LANGUAGES <codes...> [BASE <code>] [STRICT_STRINGS]]
#     [TITLE <text>] [DESCRIPTION <text>] [AUTHOR <text>] [VERSION <major.minor.micro>]
#     [ICON <48x48 png>]
#     [CIA UNIQUE_ID <0x00300-0xF7FFF> [PRODUCT_CODE <CTR-X-XXXX>] [BANNER <256x128 png>]
#          [BANNER_AUDIO <wav, at most 3 s>]])
#
# Creates a game executable linked to the engine.
#   PC : <target>.exe (plus SDL3.dll copied next to it on Windows).
#   3DS: <target>.elf and <target>.3dsx with SMDH metadata (title, description, author, icon).
# ASSETS: every file of the folder, converted and packed with the game (nnh_assets.cmake; romfs on 3DS,
# a folder next to the PC executable); read with <nnh/assets.hpp>, images with nnh::load_texture("path").
# DATA_SCHEMA (spec 014): the sources holding the game's NNH_DATA_TYPE tables, compiled into the game and into a
# host program that describes the schema (nnh_data.cmake); then every ASSETS file whose extension the asset pipeline
# doesn't own goes through `nnh-assetc data`, which converts the files whose extension is a declared key and checks
# their references (nnh_assets.cmake).
# LANGUAGES (spec 029): the languages the game ships, each with strings/<code>.strings or strings/<code>/*.strings in
# ASSETS; BASE (default: the first) is the one every key must exist in; STRICT_STRINGS makes the build's warnings
# (missing translations, length limits) errors (nnh_strings.cmake).
# IMAGES are checked, converted and embedded (nnh_images.cmake); load them with nnh::load_texture("name").
# Package (spec 009, nnh_package_rules.cmake): the metadata is checked at configure time; ICON (default:
# the NNHGE icon) at build time, on every system. TITLE becomes the default AppConfig::title. CIA options
# are stored as target properties (NNH_CIA_*); on 3DS, nnh_cia.cmake adds <target>_dist, dist and, with
# CIA, <target>_cia.

include("${CMAKE_CURRENT_LIST_DIR}/nnh_assets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/nnh_images.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/nnh_package_rules.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/nnh_cia.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/nnh_data.cmake")
set(_NNH_GAME_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(nnh_add_game target)
    cmake_parse_arguments(PARSE_ARGV 1 G "STRICT_STRINGS" "TITLE;DESCRIPTION;AUTHOR;VERSION;ICON;ASSETS;BASE"
                          "SOURCES;IMAGES;CIA;DATA_SCHEMA;LANGUAGES")
    if(NOT G_SOURCES)
        message(FATAL_ERROR "nnh_add_game(${target}): SOURCES is required")
    endif()
    if(NOT G_TITLE)
        set(G_TITLE "${target}")
    endif()
    if(NOT G_DESCRIPTION)
        set(G_DESCRIPTION "Made with NNHGE")
    endif()
    if(NOT G_AUTHOR)
        set(G_AUTHOR "Unknown")
    endif()
    set(_cia "")
    if(DEFINED G_CIA OR "CIA" IN_LIST G_KEYWORDS_MISSING_VALUES)
        set(_cia CIA ${G_CIA})
    endif()
    # Optional values are passed only when given (an empty keyword value warns under CMake 4, CMP0174).
    set(_optional "")
    foreach(_key VERSION ICON)
        if(G_${_key})
            list(APPEND _optional ${_key} "${G_${_key}}")
        endif()
    endforeach()
    nnh_resolve_package(P TARGET ${target} SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" TITLE "${G_TITLE}"
                        DESCRIPTION "${G_DESCRIPTION}" AUTHOR "${G_AUTHOR}" ${_optional} ${_cia})

    add_executable(${target} ${G_SOURCES})
    target_link_libraries(${target} PRIVATE nnh::engine)
    # The game's title is the default window title (AppConfig::title), escaped for a C string.
    string(REPLACE "\\" "\\\\" _title_c "${G_TITLE}")
    string(REPLACE "\"" "\\\"" _title_c "${_title_c}")
    target_compile_definitions(${target} PRIVATE "NNH_GAME_TITLE=\"${_title_c}\"")
    set_target_properties(${target} PROPERTIES
        NNH_TITLE "${G_TITLE}" NNH_DESCRIPTION "${G_DESCRIPTION}" NNH_AUTHOR "${G_AUTHOR}"
        NNH_VERSION_MAJOR ${P_VERSION_MAJOR} NNH_VERSION_MINOR ${P_VERSION_MINOR} NNH_VERSION_MICRO ${P_VERSION_MICRO}
        NNH_CIA ${P_CIA})
    # One line in the games manifest for the nnh tool (spec 010): name|binary dir|has a .cia|title (the
    # title, for nnh serve's game list, spec 011).
    set(_has_cia 0)
    if(P_CIA)
        set(_has_cia 1)
    endif()
    string(REGEX REPLACE "[\r\n]" " " _title_line "${G_TITLE}")
    file(APPEND "${CMAKE_BINARY_DIR}/nnh_games.txt" "${target}|${CMAKE_CURRENT_BINARY_DIR}|${_has_cia}|${_title_line}\n")
    if(P_CIA)
        set_target_properties(${target} PROPERTIES NNH_CIA_UNIQUE_ID "${P_UNIQUE_ID}"
            NNH_CIA_PRODUCT_CODE "${P_PRODUCT_CODE}" NNH_CIA_BANNER "${P_BANNER}" NNH_CIA_BANNER_AUDIO "${P_BANNER_AUDIO}")
    endif()

    # Icon check: one header read, rerun only when the icon changes.
    set(_icon_stamp "${CMAKE_CURRENT_BINARY_DIR}/${target}_icon.stamp")
    add_custom_command(OUTPUT "${_icon_stamp}"
        COMMAND ${CMAKE_COMMAND} -DICON=${P_ICON} -DSTAMP=${_icon_stamp} -P ${_NNH_GAME_CMAKE_DIR}/nnh_check_icon.cmake
        DEPENDS "${P_ICON}" "${_NNH_GAME_CMAKE_DIR}/nnh_check_icon.cmake"
        COMMENT "Icon for ${target}"
        VERBATIM)
    add_custom_target(${target}_icon DEPENDS "${_icon_stamp}")
    add_dependencies(${target} ${target}_icon)

    if(G_IMAGES)
        nnh_add_images(${target} ${G_IMAGES})
    endif()
    if(G_DATA_SCHEMA)
        nnh_add_data_schema(${target} ${G_DATA_SCHEMA}) # before the assets: their data steps need the description
    endif()
    set(_romfs "")
    if(G_LANGUAGES) # spec 029: read by the asset pipeline's string steps
        if(NOT G_ASSETS)
            message(FATAL_ERROR "nnh_add_game(${target}): LANGUAGES needs ASSETS (the string files live there)")
        endif()
        set_target_properties(${target} PROPERTIES NNH_LANGUAGES "${G_LANGUAGES}" NNH_BASE_LANGUAGE "${G_BASE}"
                                                   NNH_STRICT_STRINGS "${G_STRICT_STRINGS}")
    endif()
    if(G_ASSETS)
        nnh_add_assets(${target} "${G_ASSETS}")
        set(_romfs ROMFS ${target}_assets)
    endif()

    if(NNH_PLATFORM STREQUAL "3ds")
        ctr_generate_smdh(${target}.smdh NAME "${G_TITLE}" DESCRIPTION "${G_DESCRIPTION}" AUTHOR "${G_AUTHOR}"
                          ICON "${P_ICON}")
        ctr_create_3dsx(${target} SMDH ${target}.smdh ${_romfs})
        set(_assets_flag "")
        if(G_ASSETS)
            set(_assets_flag ASSETS)
        endif()
        nnh_add_3ds_package(${target} ${_assets_flag}) # <target>_dist, dist, <target>_cia
    elseif(WIN32)
        # Copy runtime DLLs (SDL3.dll) next to the executable so it runs from the build folder.
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS)
    endif()
endfunction()
