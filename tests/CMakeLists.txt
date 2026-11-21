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
    assets/test_asset_index.cpp
    assets/test_asset_path.cpp
    assets/test_asset_textures.cpp
    assets/test_assets.cpp
    audio/test_audio_lifecycle.cpp
    audio/test_audio_play.cpp
    audio/test_mix.cpp
    audio/test_music.cpp
    audio/test_music_track.cpp
    audio/test_ogg_reader.cpp
    audio/test_sounds.cpp
    audio/test_voice_table.cpp
    audio/test_wav_reader.cpp
    build/test_flavor.cpp
    collision/test_aabb.cpp
    collision/test_cells_touching.cpp
    collision/test_contacts.cpp
    collision/test_grid.cpp
    collision/test_grid_move.cpp
    collision/test_one_way.cpp
    core/test_crc32.cpp
    crash/test_assert.cpp
    crash/test_report.cpp
    debug/test_debug_list.cpp
    debug/test_debug_pass.cpp
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
    simulator/test_audio_mixer_pc.cpp
    simulator/test_sim_layout.cpp
    simulator/test_sim_settings.cpp
    text/test_draw_text.cpp
    text/test_font_atlas.cpp
    text/test_font_texture.cpp
    text/test_glyphs.cpp
    text/test_layout.cpp
    text/test_utf8.cpp
    time/test_clock.cpp
    time/test_duration.cpp
    time/test_fixed_step.cpp
)
target_include_directories(nnh_tests PRIVATE ${PROJECT_SOURCE_DIR}/engine/core/src)
# Sound fixtures (tests/data/sounds, made by make_sounds.py) are read from the source tree.
target_compile_definitions(nnh_tests PRIVATE NNH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_link_libraries(nnh_tests PRIVATE nnh_test_framework nnh_fuzz_targets nnh::core nnh::platform_fake)
# The PC simulator's window layout and settings are pure (SDL-free) and tested here (spec 006).
set(_pc_src ${PROJECT_SOURCE_DIR}/engine/platforms/pc/src)
target_sources(nnh_tests PRIVATE ${_pc_src}/audio_mixer_pc.cpp ${_pc_src}/sim_layout.cpp ${_pc_src}/sim_settings.cpp)
target_include_directories(nnh_tests PRIVATE ${_pc_src})
# The 3DS homebrew loader's argument block is pure code too (spec 011 FR-4).
set(_3ds_src ${PROJECT_SOURCE_DIR}/engine/platforms/3ds/src)
target_sources(nnh_tests PRIVATE platform_3ds/test_hbldr_args.cpp ${_3ds_src}/hbldr_args.cpp)
target_include_directories(nnh_tests PRIVATE ${_3ds_src})
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
# Headless and quiet: no display, no sound card needed (the dummy audio driver still runs the backend).
set_tests_properties(conformance_pc PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;SDL_AUDIO_DRIVER=dummy")

# 3DS extension headers must refuse to compile for other platforms (FR-P5).
if(NOT MSVC)
    add_test(NAME extension_header_rejected_off_platform
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only
                -I${PROJECT_SOURCE_DIR}/engine/platforms/3ds/include
                ${CMAKE_CURRENT_SOURCE_DIR}/system/compile_fail_3ds_extension.cpp)
    set_tests_properties(extension_header_rejected_off_platform PROPERTIES WILL_FAIL TRUE)
    # The same for the NNH Link headers (spec 011), checking the #error text (a missing file fails too).
    foreach(_ext network launch)
        add_test(NAME extension_${_ext}_rejected_off_platform
            COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only
                    -I${PROJECT_SOURCE_DIR}/engine/platforms/3ds/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/system/compile_fail_3ds_${_ext}.cpp)
        set_tests_properties(extension_${_ext}_rejected_off_platform PROPERTIES
            PASS_REGULAR_EXPRESSION "is only available when building for Nintendo 3DS")
    endforeach()

    # Event handlers with captures larger than the inline storage must not compile (spec 001 FR-2).
    add_test(NAME large_handler_capture_rejected
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only -I${PROJECT_SOURCE_DIR}/engine/core/include
                ${CMAKE_CURRENT_SOURCE_DIR}/events/compile_fail_big_capture.cpp)
    set_tests_properties(large_handler_capture_rejected PROPERTIES WILL_FAIL TRUE)

    # AppConfig::title defaults to the game's TITLE (spec 009 FR-1); the last case proves the check bites.
    set(_title_check ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only -I${PROJECT_SOURCE_DIR}/engine/core/include)
    add_test(NAME game_title_default COMMAND ${_title_check} ${CMAKE_CURRENT_SOURCE_DIR}/app/compile_game_title.cpp)
    add_test(NAME game_title_from_build COMMAND ${_title_check} "-DNNH_GAME_TITLE=\"My Game\""
             "-DEXPECTED_TITLE=\"My Game\"" ${CMAKE_CURRENT_SOURCE_DIR}/app/compile_game_title.cpp)
    add_test(NAME game_title_mismatch_fails COMMAND ${_title_check} "-DNNH_GAME_TITLE=\"My Game\""
             ${CMAKE_CURRENT_SOURCE_DIR}/app/compile_game_title.cpp)
    set_tests_properties(game_title_mismatch_fails PROPERTIES WILL_FAIL TRUE)
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

# Sound check of nnh_add_game(... ASSETS ...) (spec 008 FR-1): fixtures from data/sounds/make_sounds.py.
set(_sounds ${CMAKE_CURRENT_SOURCE_DIR}/data/sounds)
set(_sound_check ${PROJECT_SOURCE_DIR}/cmake/nnh_check_sound.cmake)
foreach(file ok_mono16.wav sweep_mono_22k.ogg sweep_stereo_32k_loop.ogg)
    add_test(NAME sound_check_accepts_${file} COMMAND ${CMAKE_COMMAND} -DSOUND=${_sounds}/${file} -P ${_sound_check})
endforeach()
foreach(case "bad_24bit.wav|bad_24bit.wav is 24-bit"
             "bad_3ch.wav|bad_3ch.wav has 3 channels"
             "bad_96k.wav|bad_96k.wav is at 96000 Hz"
             "bad_not_ogg.ogg|bad_not_ogg.ogg: not an Ogg file"
             "missing.wav|missing.wav: file not found")
    string(REPLACE "|" ";" case "${case}")
    list(GET case 0 file)
    list(GET case 1 expected)
    add_test(NAME sound_check_rejects_${file} COMMAND ${CMAKE_COMMAND} -DSOUND=${_sounds}/${file} -P ${_sound_check})
    set_tests_properties(sound_check_rejects_${file} PROPERTIES WILL_FAIL TRUE)
    add_test(NAME sound_check_explains_${file} COMMAND ${CMAKE_COMMAND} -DSOUND=${_sounds}/${file} -P ${_sound_check})
    set_tests_properties(sound_check_explains_${file} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
endforeach()

# Packaging checks of nnh_add_game() (spec 009 FR-2, FR-5): fixtures from data/package/make_fixtures.py.
# nnh_script_check(<name> <script> <expected message or OK> <-D args...>): OK = must pass; otherwise it
# must fail AND print the message.
function(nnh_script_check name script expected)
    if(expected STREQUAL "OK")
        add_test(NAME ${name} COMMAND ${CMAKE_COMMAND} ${ARGN} -P ${script})
    else()
        add_test(NAME ${name}_fails COMMAND ${CMAKE_COMMAND} ${ARGN} -P ${script})
        set_tests_properties(${name}_fails PROPERTIES WILL_FAIL TRUE)
        add_test(NAME ${name}_explains COMMAND ${CMAKE_COMMAND} ${ARGN} -P ${script})
        set_tests_properties(${name}_explains PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
    endif()
endfunction()
set(_pkg ${CMAKE_CURRENT_SOURCE_DIR}/data/package)
set(_cm ${PROJECT_SOURCE_DIR}/cmake)
nnh_script_check(icon_check_48 ${_cm}/nnh_check_icon.cmake OK -DICON=${_pkg}/icon_48.png)
nnh_script_check(icon_check_64 ${_cm}/nnh_check_icon.cmake "icon_64.png is 64x64: the icon must be a 48x48 PNG" -DICON=${_pkg}/icon_64.png)
nnh_script_check(icon_check_not_png ${_cm}/nnh_check_icon.cmake "not_png.png: not a PNG file" -DICON=${_pkg}/not_png.png)
nnh_script_check(banner_check_ok ${_cm}/nnh_check_banner.cmake OK -DBANNER=${_pkg}/banner_256x128.png -DAUDIO=${_pkg}/jingle_ok.wav)
nnh_script_check(banner_check_3s ${_cm}/nnh_check_banner.cmake OK -DAUDIO=${_pkg}/jingle_3s.wav)
nnh_script_check(banner_check_size ${_cm}/nnh_check_banner.cmake "banner_200x120.png is 200x120: the banner must be a 256x128 PNG" -DBANNER=${_pkg}/banner_200x120.png)
nnh_script_check(banner_check_long ${_cm}/nnh_check_banner.cmake "jingle_long.wav lasts 3.2 s: the banner sound can last at most 3 s" -DAUDIO=${_pkg}/jingle_long.wav)
nnh_script_check(banner_check_24bit ${_cm}/nnh_check_banner.cmake "jingle_24bit.wav is 24-bit: the banner sound must be a 16-bit PCM WAV" -DAUDIO=${_pkg}/jingle_24bit.wav)
nnh_script_check(banner_check_not_wav ${_cm}/nnh_check_banner.cmake "icon_48.png: not a WAV file" -DAUDIO=${_pkg}/icon_48.png)
# Metadata: tests/data/package/check_package.cmake calls nnh_check_package_values() with -D values.
set(_ok -DUNIQUE_ID=0xE0A01 -DPRODUCT_CODE=CTR-H-AUDI -DVERSION=1.2.3 -DTITLE=Game -DDESCRIPTION=Desc -DAUTHOR=Me)
set(_check ${_pkg}/check_package.cmake)
string(REPEAT "a" 64 _a64)
string(REPEAT "a" 65 _a65)
string(REPEAT "a" 128 _a128)
string(REPEAT "a" 129 _a129)
string(REPEAT "ç" 64 _c64) # 64 characters, 128 bytes
nnh_script_check(package_check_ok ${_check} OK ${_ok})
nnh_script_check(package_check_limits ${_check} OK ${_ok} -DTITLE=${_a64} -DDESCRIPTION=${_a128} -DAUTHOR=${_a64} -DUNIQUE_ID=0x00300 -DVERSION=63.63.15)
nnh_script_check(package_check_utf8 ${_check} OK ${_ok} -DTITLE=${_c64})
nnh_script_check(package_check_id_low ${_check} "UNIQUE_ID 0x2FF is outside the homebrew range 0x00300-0xF7FFF" ${_ok} -DUNIQUE_ID=0x2FF)
nnh_script_check(package_check_id_high ${_check} "UNIQUE_ID 0xF8000 is outside the homebrew range" ${_ok} -DUNIQUE_ID=0xF8000)
nnh_script_check(package_check_id_hex ${_check} "UNIQUE_ID 123 must be hexadecimal" ${_ok} -DUNIQUE_ID=123)
nnh_script_check(package_check_code ${_check} "PRODUCT_CODE CTR-H-abc must look like CTR-H-ABCD" ${_ok} -DPRODUCT_CODE=CTR-H-abc)
nnh_script_check(package_check_version ${_check} "VERSION 1.2 must be major.minor.micro" ${_ok} -DVERSION=1.2)
nnh_script_check(package_check_major ${_check} "VERSION 64.0.0 must be major.minor.micro" ${_ok} -DVERSION=64.0.0)
nnh_script_check(package_check_micro ${_check} "VERSION 1.0.16 must be major.minor.micro" ${_ok} -DVERSION=1.0.16)
nnh_script_check(package_check_title ${_check} "TITLE is 65 characters: at most 64" ${_ok} -DTITLE=${_a65})
nnh_script_check(package_check_title_utf8 ${_check} "TITLE is 65 characters" ${_ok} -DTITLE=${_c64}x)
nnh_script_check(package_check_description ${_check} "DESCRIPTION is 129 characters: at most 128" ${_ok} -DDESCRIPTION=${_a129})
nnh_script_check(package_check_author ${_check} "AUTHOR is 65 characters: at most 64" ${_ok} -DAUTHOR=${_a65})

# Resolving nnh_add_game()'s package options (spec 009 FR-1, FR-3, FR-5): defaults, CIA group, errors.
set(_resolve ${_pkg}/resolve_package.cmake)
nnh_script_check(package_resolve_defaults ${_resolve} OK -DTARGET=hello)
set_tests_properties(package_resolve_defaults PROPERTIES PASS_REGULAR_EXPRESSION
    "version=0[.]1[.]0 icon=default_icon[.]png cia=FALSE")
nnh_script_check(package_resolve_cia ${_resolve} OK -DTARGET=audio -DVERSION=1.2.3 -DICON=icon_48.png "-DCIA=UNIQUE_ID,0xE0A01")
set_tests_properties(package_resolve_cia PROPERTIES PASS_REGULAR_EXPRESSION
    "version=1[.]2[.]3 icon=icon_48[.]png cia=TRUE id=0xE0A01 code=CTR-H-AUDI banner=default_banner[.]png audio=default_banner[.]wav")
nnh_script_check(package_resolve_short_name ${_resolve} OK -DTARGET=a_b "-DCIA=UNIQUE_ID,0x00400")
set_tests_properties(package_resolve_short_name PROPERTIES PASS_REGULAR_EXPRESSION "code=CTR-H-ABXX")
nnh_script_check(package_resolve_own_banner ${_resolve} OK -DTARGET=x
    "-DCIA=UNIQUE_ID,0x00400,PRODUCT_CODE,CTR-P-GAME,BANNER,banner_256x128.png,BANNER_AUDIO,jingle_ok.wav")
set_tests_properties(package_resolve_own_banner PROPERTIES PASS_REGULAR_EXPRESSION
    "code=CTR-P-GAME banner=banner_256x128[.]png audio=jingle_ok[.]wav")
nnh_script_check(package_resolve_no_id ${_resolve} "CIA needs UNIQUE_ID" -DTARGET=x "-DCIA=PRODUCT_CODE,CTR-P-GAME")
nnh_script_check(package_resolve_unknown ${_resolve} "unknown CIA option: SYSTEM_MOD" -DTARGET=x "-DCIA=UNIQUE_ID,0x400,SYSTEM_MOD,64MB")
nnh_script_check(package_resolve_no_banner ${_resolve} "BANNER not found: missing[.]png" -DTARGET=x "-DCIA=UNIQUE_ID,0x400,BANNER,missing.png")
nnh_script_check(package_resolve_no_icon ${_resolve} "ICON not found: nope[.]png" -DTARGET=x -DICON=nope.png)
nnh_script_check(package_resolve_bad_version ${_resolve} "VERSION 2 must be major[.]minor[.]micro" -DTARGET=x -DVERSION=2)
nnh_script_check(package_resolve_long_title ${_resolve} "TITLE is 65 characters" -DTARGET=x -DTITLE=${_a65})

# Asset index of nnh_add_game(... ASSETS ...) (spec 007 FR-1, FR-6): sorted output; bad names fail the
# build with a message naming the file.
set(_lists ${CMAKE_CURRENT_SOURCE_DIR}/data/asset_lists)
set(_index_script ${PROJECT_SOURCE_DIR}/cmake/nnh_asset_index.cmake)
add_test(NAME asset_index_sorts_valid_names
    COMMAND ${CMAKE_COMMAND} -DDIR=${_lists} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/asset_index_valid.txt
            -DSCRIPT=${_index_script} -P ${_lists}/check_valid.cmake)
add_test(NAME asset_index_removes_stale_outputs
    COMMAND ${CMAKE_COMMAND} -DDIR=${_lists} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/asset_index_stale
            -DSCRIPT=${_index_script} -P ${_lists}/check_stale.cmake)
foreach(case "dotdot|no '.' or '..' folders"
             "chars|use only letters"
             "reserved|reserved"
             "clash|differ only by letter case")
    string(REPLACE "|" ";" case "${case}")
    list(GET case 0 name)
    list(GET case 1 expected)
    add_test(NAME asset_index_rejects_${name}
        COMMAND ${CMAKE_COMMAND} -DLIST=${_lists}/${name}.list -DOUT=${CMAKE_CURRENT_BINARY_DIR}/asset_index_${name}.txt
                -P ${_index_script})
    set_tests_properties(asset_index_rejects_${name} PROPERTIES WILL_FAIL TRUE)
    add_test(NAME asset_index_explains_${name}
        COMMAND ${CMAKE_COMMAND} -DLIST=${_lists}/${name}.list -DOUT=${CMAKE_CURRENT_BINARY_DIR}/asset_index_${name}.txt
                -P ${_index_script})
    set_tests_properties(asset_index_explains_${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
endforeach()

# libFuzzer executables (clang only): cmake --preset pc-debug -DNNH_BUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
# then run e.g. build/pc-debug/tests/fuzz_save -max_total_time=60
option(NNH_BUILD_FUZZERS "Build libFuzzer targets (requires clang)" OFF)
if(NNH_BUILD_FUZZERS)
    # The code under test lives in these libraries: instrument it for coverage and ASan, or libFuzzer
    # mutates blind (it reported "cov: 1" before this was added).
    foreach(lib nnh_core nnh_stb_vorbis nnh_fuzz_targets)
        target_compile_options(${lib} PRIVATE -fsanitize=fuzzer-no-link,address)
    endforeach()
    foreach(name path save asset_index wav ogg)
        add_executable(fuzz_${name} fuzz/libfuzzer_${name}.cpp)
        target_link_libraries(fuzz_${name} PRIVATE nnh_fuzz_targets)
        target_compile_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address)
    endforeach()
endif()
