# Tests (docs/process/tdd.md). Layout mirrors engine/core/src: tests/<module>/test_<concept>.cpp.
# List new files here (explicit lists, no file(GLOB)).

add_library(nnh_test_framework STATIC framework/test_main.cpp)
target_include_directories(nnh_test_framework PUBLIC framework)
target_compile_features(nnh_test_framework PUBLIC cxx_std_20)

# Fuzz target bodies, shared by the unit tests (mini-fuzz) and the libFuzzer executables below.
add_library(nnh_fuzz_targets STATIC fuzz/fuzz_targets.cpp)
target_include_directories(nnh_fuzz_targets PUBLIC fuzz PRIVATE ${PROJECT_SOURCE_DIR}/engine/core/src)
target_link_libraries(nnh_fuzz_targets PUBLIC nnh::core nnh::platform_fake)

# Unit tests: nnh::core driven through the fake platform.
add_executable(nnh_tests
    app/test_data_folder.cpp
    app/test_frame_loop.cpp
    build/test_flavor.cpp
    crash/test_assert.cpp
    crash/test_report.cpp
    debug/test_debug_list.cpp
    debug/test_overlay.cpp
    debug/test_profile.cpp
    events/test_capacities.cpp
    events/test_dispatch.cpp
    events/test_event_queue.cpp
    events/test_input_events.cpp
    events/test_overflow.cpp
    events/test_reentrancy.cpp
    events/test_small_function.cpp
    events/test_subscriber_table.cpp
    events/test_subscriptions.cpp
    events/test_type_id.cpp
    fs/test_files.cpp
    fs/test_log_files.cpp
    fs/test_path.cpp
    fs/test_save.cpp
    fuzz/test_fuzz_smoke.cpp
    gfx/test_draw_calls.cpp
    gfx/test_draw_list.cpp
    gfx/test_texture_rules.cpp
    gfx/test_texture_table.cpp
    gfx/test_textures.cpp
    gfx/test_images.cpp
    input/test_dpad_mirror.cpp
    input/test_input_queries.cpp
    input/test_input_state.cpp
    input/test_stick.cpp
    input/test_touch_state.cpp
    log/test_format.cpp
    log/test_log.cpp
    log/test_ring.cpp
    math/test_vec2.cpp
    memory/test_memory.cpp
    result/test_result.cpp
    scene/test_run.cpp
    scene/test_scene_stack.cpp
    system/test_system.cpp
    text/test_builtin_font.cpp
    text/test_glyphs.cpp
    text/test_layout.cpp
    text/test_utf8.cpp
    time/test_clock.cpp
    time/test_duration.cpp
    time/test_fixed_step.cpp
)
target_include_directories(nnh_tests PRIVATE ${PROJECT_SOURCE_DIR}/engine/core/src)
target_link_libraries(nnh_tests PRIVATE nnh_test_framework nnh_fuzz_targets nnh::core nnh::platform_fake)
add_test(NAME unit COMMAND nnh_tests)

# Platform contract conformance: the same tests against each platform library.
function(nnh_add_conformance platform)
    set(target nnh_conformance_${platform})
    add_executable(${target} platform_conformance/test_contract.cpp)
    target_link_libraries(${target} PRIVATE nnh_test_framework nnh::core_contract nnh::core
                                            nnh::platform_${platform})
    add_test(NAME conformance_${platform} COMMAND ${target})
    if(WIN32 AND platform STREQUAL "pc")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS)
    endif()
endfunction()

nnh_add_conformance(fake)
nnh_add_conformance(pc)
# The PC platform runs headless in tests (no window on screen, works in CI).
set_tests_properties(conformance_pc PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software")

# 3DS extension headers must refuse to compile for other platforms (FR-P5).
if(NOT MSVC)
    add_test(NAME extension_header_rejected_off_platform
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only
                -I${PROJECT_SOURCE_DIR}/engine/platforms/3ds/include
                ${CMAKE_CURRENT_SOURCE_DIR}/system/compile_fail_3ds_extension.cpp)
    set_tests_properties(extension_header_rejected_off_platform PROPERTIES WILL_FAIL TRUE)

    # Event handlers with captures larger than the inline storage must not compile (spec 001 FR-2).
    add_test(NAME large_handler_capture_rejected
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only -I${PROJECT_SOURCE_DIR}/engine/core/include
                ${CMAKE_CURRENT_SOURCE_DIR}/events/compile_fail_big_capture.cpp)
    set_tests_properties(large_handler_capture_rejected PROPERTIES WILL_FAIL TRUE)
endif()

# Image check of nnh_add_game(... IMAGES ...) (spec 004 FR-3, AC-4): run the script on fixture images.
set(_images ${CMAKE_CURRENT_SOURCE_DIR}/data/images)
set(_check ${PROJECT_SOURCE_DIR}/cmake/nnh_check_image.cmake)
add_test(NAME image_check_accepts_power_of_two
    COMMAND ${CMAKE_COMMAND} -DIMAGE=${_images}/ok_16x8.png -P ${_check})
foreach(case "npot_100x60.png|npot_100x60.png is 100x60: textures need power-of-two"
             "big_2048x8.png|big_2048x8.png is 2048x8"
             "not_png.png|not_png.png: not a PNG file"
             "missing.png|missing.png: file not found")
    string(REPLACE "|" ";" case "${case}")
    list(GET case 0 file)
    list(GET case 1 expected)
    add_test(NAME image_check_rejects_${file}
        COMMAND ${CMAKE_COMMAND} -DIMAGE=${_images}/${file} -P ${_check})
    # Must fail, and say why (with the file name).
    set_tests_properties(image_check_rejects_${file} PROPERTIES WILL_FAIL TRUE)
    add_test(NAME image_check_explains_${file}
        COMMAND ${CMAKE_COMMAND} -DIMAGE=${_images}/${file} -P ${_check})
    set_tests_properties(image_check_explains_${file} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
endforeach()

# libFuzzer executables (clang only): cmake --preset pc-debug -DNNH_BUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
# then run e.g. build/pc-debug/tests/fuzz_save -max_total_time=60
option(NNH_BUILD_FUZZERS "Build libFuzzer targets (requires clang)" OFF)
if(NNH_BUILD_FUZZERS)
    foreach(name path save)
        add_executable(fuzz_${name} fuzz/libfuzzer_${name}.cpp)
        target_link_libraries(fuzz_${name} PRIVATE nnh_fuzz_targets)
        target_compile_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address)
    endforeach()
endif()
