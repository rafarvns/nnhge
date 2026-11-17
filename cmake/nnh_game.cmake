# nnh_add_game(<target>
#     SOURCES <files...>
#     [TITLE <text>] [DESCRIPTION <text>] [AUTHOR <text>] [ICON <48x48 png>])
#
# Creates a game executable linked to the engine.
#   PC : <target>.exe (plus SDL3.dll copied next to it on Windows).
#   3DS: <target>.elf and <target>.3dsx with SMDH metadata (title, description, author, icon).

function(nnh_add_game target)
    cmake_parse_arguments(PARSE_ARGV 1 G "" "TITLE;DESCRIPTION;AUTHOR;ICON" "SOURCES")
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

    if(NNH_PLATFORM STREQUAL "3ds")
        set(_smdh_args NAME "${G_TITLE}" DESCRIPTION "${G_DESCRIPTION}" AUTHOR "${G_AUTHOR}")
        if(G_ICON)
            list(APPEND _smdh_args ICON "${G_ICON}")
        endif()
        ctr_generate_smdh(${target}.smdh ${_smdh_args})
        ctr_create_3dsx(${target} SMDH ${target}.smdh)
    elseif(WIN32)
        # Copy runtime DLLs (SDL3.dll) next to the executable so it runs from the build folder.
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS)
    endif()
endfunction()
