# nnh — the NNHGE command-line tool (spec 010): build, run on the console with readable logs, install.
# A host program built with the PC presets; never a dependency of games. Explicit source lists.

add_library(nnh_tool_lib STATIC
    src/cli.cpp
    src/commands/build.cpp
    src/commands/install.cpp
    src/commands/run.cpp
    src/console_link.cpp
    src/fbi.cpp
    src/file_server.cpp
    src/log_listener.cpp
    src/log_view.cpp
    src/manifest.cpp
    src/netloader.cpp
    src/repo.cpp
    src/terminal.cpp
    src/user_config.cpp
    src/zlib_stored.cpp
)
target_include_directories(nnh_tool_lib PUBLIC src)
nnh_engine_options(nnh_tool_lib)
find_package(Threads REQUIRED) # the HTTP server runs on a thread
target_link_libraries(nnh_tool_lib PUBLIC Threads::Threads nnh_link)

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
        tests/test_console_link.cpp
        tests/test_fbi.cpp
        tests/test_log_view.cpp
        tests/test_manifest.cpp
        tests/test_netloader.cpp
        tests/test_repo.cpp
        tests/test_user_config.cpp
        tests/test_zlib_stored.cpp
    )
    target_link_libraries(nnh_tool_tests PRIVATE nnh_tool_lib nnh_test_framework)
    target_compile_definitions(nnh_tool_tests PRIVATE NNH_TOOL_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}"
                                                     NNH_TOOL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    nnh_engine_options(nnh_tool_tests)
    _nnh_tool_static_runtime(nnh_tool_tests)
    add_test(NAME nnh_tool COMMAND nnh_tool_tests)
endif()
