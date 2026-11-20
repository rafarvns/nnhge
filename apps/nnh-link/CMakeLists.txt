# NNH Link (spec 011): the console app that asks `nnh serve` to build a game, receives it and starts it.
# An NNHGE game plus the nnh_link library. Explicit source lists; the platform file is picked here.

set(_nnh_link_logic src/device.cpp src/layout.cpp src/link_state.cpp src/net_client.cpp src/pad.cpp src/storage.cpp)

nnh_add_game(nnh-link
    SOURCES src/main.cpp src/link_scene.cpp src/demo.cpp src/storage_file.cpp
            src/view_lists.cpp src/view_pad.cpp src/view_progress.cpp
            src/platform_link_${NNH_PLATFORM}.cpp ${_nnh_link_logic}
    TITLE "NNH Link"
    DESCRIPTION "Build and run NNHGE games from the console"
    AUTHOR "NNHGE contributors")
target_link_libraries(nnh-link PRIVATE nnh_link)
target_include_directories(nnh-link PRIVATE src)

if(NNH_BUILD_TESTS)
    # The logic, the storage, the pad and the device files against fakes (no engine, no network).
    add_executable(nnh_link_app_tests
        tests/test_device.cpp tests/test_layout.cpp tests/test_link_state.cpp tests/test_pad.cpp tests/test_storage.cpp
        src/platform_link_pc.cpp ${_nnh_link_logic})
    target_include_directories(nnh_link_app_tests PRIVATE src tests)
    target_link_libraries(nnh_link_app_tests PRIVATE nnh_link nnh_test_framework)
    nnh_engine_options(nnh_link_app_tests)
    if(MINGW)
        target_link_options(nnh_link_app_tests PRIVATE -static) # see tools/nnh/CMakeLists.txt
    endif()
    add_test(NAME nnh_link_app COMMAND nnh_link_app_tests)
endif()
