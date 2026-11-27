# nnh — the NNHGE command-line tool (spec 010): build, run on the console with readable logs, install;
# serve for NNH Link (spec 011).
# A host program built with the PC presets; never a dependency of games. Explicit source lists.

add_library(nnh_tool_lib STATIC
    src/cli.cpp
    src/commands/build.cpp
    src/commands/install.cpp
    src/commands/project.cpp
    src/commands/run.cpp
    src/commands/run_pc.cpp
    src/commands/run_reload.cpp
    src/commands/serve.cpp
    src/console_link.cpp
    src/fbi.cpp
    src/file_server.cpp
    src/game_log.cpp
    src/host_compiler.cpp
    src/log_listener.cpp
    src/log_view.cpp
    src/manifest.cpp
    src/netloader.cpp
    src/preset_names.cpp
    src/project_file.cpp
    src/project_root.cpp
    src/reload/asset_build.cpp
    src/reload/asset_swap.cpp
    src/reload/batch_sender.cpp
    src/reload/output_snapshot.cpp
    src/reload/interrupt.cpp
    src/reload/paced_units.cpp
    src/reload/reload_controller.cpp
    src/reload/reload_setup.cpp
    src/reload/source_watcher.cpp
    src/remote/game_records.cpp
    src/remote/game_session.cpp
    src/remote/live_catalog.cpp
    src/remote/live_text.cpp
    src/remote/game_stream.cpp
    src/remote/log_match.cpp
    src/remote/png_writer.cpp
    src/remote/screen_image.cpp
    src/remote/script_interactive.cpp
    src/remote/script_live.cpp
    src/remote/script_parser.cpp
    src/remote/script_runner.cpp
    src/remote/script_runner_reload.cpp
    src/remote/script_runner_swap.cpp
    src/remote/script_reload.cpp
    src/remote/script_runner_live.cpp
    src/remote/script_timeline.cpp
    src/remote/script_tokens.cpp
    src/remote/stdin_lines.cpp
    src/remote/tunes_file.cpp
    src/repo.cpp
    src/serve/discovery_responder.cpp
    src/serve/pairing_codes.cpp
    src/serve/pairs_file.cpp
    src/serve/serve_hooks.cpp
    src/terminal.cpp
    src/text_file.cpp
    src/user_config.cpp
    src/zlib_stored.cpp
    # pure core files (no engine includes), shared with the console (spec 013, D28)
    ${PROJECT_SOURCE_DIR}/engine/core/src/core/crc32.cpp
    ${PROJECT_SOURCE_DIR}/engine/core/src/remote/remote_wire.cpp
    ${PROJECT_SOURCE_DIR}/engine/core/src/system/language_tag.cpp
    # live values (spec 030): names, stepping, value text and the live messages, the same code as the game's
    ${PROJECT_SOURCE_DIR}/engine/core/src/live/live_format.cpp
    ${PROJECT_SOURCE_DIR}/engine/core/src/live/live_names.cpp
    ${PROJECT_SOURCE_DIR}/engine/core/src/live/live_steps.cpp
    ${PROJECT_SOURCE_DIR}/engine/core/src/remote/live_wire.cpp
    # hot reload (spec 031): the reload messages and records, the same code as the game's
    ${PROJECT_SOURCE_DIR}/engine/core/src/core/crc32_update.cpp
    ${PROJECT_SOURCE_DIR}/engine/core/src/reload/reload_names.cpp
    ${PROJECT_SOURCE_DIR}/engine/core/src/reload/reload_overlay.cpp
    ${PROJECT_SOURCE_DIR}/engine/core/src/reload/reload_wire.cpp
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${PROJECT_SOURCE_DIR}/engine/core/src/live/live_steps.cpp
                                TARGET_DIRECTORY nnh_tool_lib PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
target_include_directories(nnh_tool_lib PUBLIC src ${PROJECT_SOURCE_DIR}/engine/core/src)
nnh_engine_options(nnh_tool_lib)
find_package(Threads REQUIRED) # the HTTP server runs on a thread
target_link_libraries(nnh_tool_lib PUBLIC Threads::Threads nnh_link PRIVATE nnh_ryu)
target_include_directories(nnh_tool_lib PUBLIC ${PROJECT_SOURCE_DIR}/engine/core/include)

add_executable(nnh src/main.cpp)
target_link_libraries(nnh PRIVATE nnh_tool_lib)
nnh_engine_options(nnh)

# MinGW: link the C++ runtime statically. nnh is a standalone program, and another libstdc++-6.dll
# earlier on the PATH (Git for Windows ships an older one) would otherwise fail to load it.
function(_nnh_tool_static_runtime target)
    if(MINGW)
        target_link_options(${target} PRIVATE -static)
    endif()
endfunction()
_nnh_tool_static_runtime(nnh)

if(NNH_BUILD_TESTS)
    add_executable(nnh_tool_tests
        tests/test_cli.cpp
        tests/test_configure_command.cpp
        tests/test_console_link.cpp
        tests/test_fbi.cpp
        tests/test_fuzz_tool.cpp
        tests/test_game_session.cpp
        tests/test_game_stream.cpp
        tests/test_log_match.cpp
        tests/test_log_view.cpp
        tests/test_manifest.cpp
        tests/test_netloader.cpp
        tests/test_png_writer.cpp
        tests/test_preset_names.cpp
        tests/test_project_file.cpp
        tests/test_project_commands.cpp
        tests/test_project_root.cpp
        tests/test_reload_controller.cpp
        tests/test_reload_tool.cpp
        tests/test_repo.cpp
        tests/test_screen_image.cpp
        tests/test_live_tool.cpp
        tests/test_script_interactive.cpp
        tests/test_script_live.cpp
        tests/test_script_parser.cpp
        tests/test_script_reload.cpp
        tests/test_script_runner.cpp
        tests/test_script_stereo.cpp
        tests/test_script_timeline.cpp
        tests/test_serve.cpp
        tests/test_stdin_lines.cpp
        tests/test_user_config.cpp
        tests/test_zlib_stored.cpp
    )
    target_link_libraries(nnh_tool_tests PRIVATE nnh_tool_lib nnh_test_framework)
    target_compile_definitions(nnh_tool_tests PRIVATE NNH_TOOL_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}"
                                                     NNH_TOOL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nnh_engine_options(nnh_tool_tests)
    _nnh_tool_static_runtime(nnh_tool_tests)
    add_test(NAME nnh_tool COMMAND nnh_tool_tests)

    # The console remote end to end in the simulator (spec 013 FR-4): nnh runs examples/input's test script
    # against this preset's build (NNH_PRESET = this build folder's name), headless. Release builds have no
    # remote control, so there is nothing to run there.
    if(TARGET input AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        get_filename_component(_nnh_preset "${PROJECT_BINARY_DIR}" NAME)
        add_test(NAME nnh_script_pc_smoke
                 COMMAND ${CMAKE_COMMAND} -DNNH=$<TARGET_FILE:nnh> -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/script_pc_smoke.cmake)
        set_tests_properties(nnh_script_pc_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_nnh_preset}"
            TIMEOUT 180)
    endif()
endif()
