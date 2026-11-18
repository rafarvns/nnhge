# nnh_add_game(<target>
#     SOURCES <files...>
#     [ASSETS <folder>]
#     [IMAGES <png files...>]
#     [TITLE <text>] [DESCRIPTION <text>] [AUTHOR <text>] [ICON <48x48 png>])
#
# Creates a game executable linked to the engine.
#   PC : <target>.exe (plus SDL3.dll copied next to it on Windows).
#   3DS: <target>.elf and <target>.3dsx with SMDH metadata (title, description, author, icon).
# ASSETS: every file of the folder, converted and packed with the game (nnh_assets.cmake; romfs on 3DS,
# a folder next to the PC executable); read with <nnh/assets.hpp>, images with nnh::load_texture("path").
# IMAGES are checked, converted and embedded (nnh_images.cmake); load them with nnh::load_texture("name").

include("${CMAKE_CURRENT_LIST_DIR}/nnh_assets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/nnh_images.cmake")

function(nnh_add_game target)
    cmake_parse_arguments(PARSE_ARGV 1 G "" "TITLE;DESCRIPTION;AUTHOR;ICON;ASSETS" "SOURCES;IMAGES")
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

    add_executable(${target} ${G_SOURCES})
    target_link_libraries(${target} PRIVATE nnh::engine)
    if(G_IMAGES)
        nnh_add_images(${target} ${G_IMAGES})
    endif()
    set(_romfs "")
    if(G_ASSETS)
        nnh_add_assets(${target} "${G_ASSETS}")
        set(_romfs ROMFS ${target}_assets)
    endif()

    if(NNH_PLATFORM STREQUAL "3ds")
        set(_smdh_args NAME "${G_TITLE}" DESCRIPTION "${G_DESCRIPTION}" AUTHOR "${G_AUTHOR}")
        if(G_ICON)
            list(APPEND _smdh_args ICON "${G_ICON}")
        endif()
        ctr_generate_smdh(${target}.smdh ${_smdh_args})
        ctr_create_3dsx(${target} SMDH ${target}.smdh ${_romfs})
    elseif(WIN32)
        # Copy runtime DLLs (SDL3.dll) next to the executable so it runs from the build folder.
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS)
    endif()
endfunction()
