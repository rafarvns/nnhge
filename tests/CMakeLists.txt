# Tests (docs/process/tdd.md). Layout mirrors engine/core/src: tests/<module>/test_<concept>.cpp.
# List new files here (explicit lists, no file(GLOB)).

add_library(nnh_test_framework STATIC framework/test_main.cpp)
target_include_directories(nnh_test_framework PUBLIC framework)
target_compile_features(nnh_test_framework PUBLIC cxx_std_20)

# Fuzz target bodies, shared by the unit tests (mini-fuzz) and the libFuzzer executables below.
add_library(nnh_fuzz_targets STATIC fuzz/fuzz_bindings.cpp fuzz/fuzz_data.cpp fuzz/fuzz_data_index.cpp fuzz/fuzz_sprite.cpp
                                     fuzz/fuzz_targets.cpp fuzz/fuzz_tilemap.cpp fuzz/fuzz_world_ops.cpp
                                     entities/model_driver.cpp fuzz/fuzz_prefab_ops.cpp
                                     fuzz/fuzz_hierarchy_ops.cpp fuzz/fuzz_transform.cpp
                                     ${PROJECT_SOURCE_DIR}/examples/prefabs/self_check.cpp
                                     ${PROJECT_SOURCE_DIR}/examples/prefabs/self_check_defs.cpp
                                     ${PROJECT_SOURCE_DIR}/examples/prefabs/self_check_sides.cpp
                                     ${PROJECT_SOURCE_DIR}/examples/hierarchy/self_check.cpp
                                     ${PROJECT_SOURCE_DIR}/examples/hierarchy/self_check_defs.cpp
                                     ${PROJECT_SOURCE_DIR}/examples/hierarchy/self_check_sides.cpp)
target_include_directories(nnh_fuzz_targets PUBLIC fuzz entities PRIVATE ${PROJECT_SOURCE_DIR}/engine/core/src
                                                                         ${PROJECT_SOURCE_DIR}/examples)
target_link_libraries(nnh_fuzz_targets PUBLIC nnh::core nnh::platform_fake)

# Unit tests: nnh::core driven through the fake platform.
add_executable(nnh_tests
    framework/alloc_counter.cpp
    actions/test_action_alloc.cpp
    actions/test_action_blocking.cpp
    actions/test_action_combine.cpp
    actions/test_action_conflicts.cpp
    actions/test_action_contexts.cpp
    actions/test_action_debug.cpp
    actions/test_action_declare.cpp
    actions/test_action_determinism.cpp
    actions/test_action_edit.cpp
    actions/test_action_eval_golden.cpp
    actions/test_action_events.cpp
    actions/test_action_fallback.cpp
    actions/test_action_hook.cpp
    actions/test_action_kinds.cpp
    actions/test_action_listen.cpp
    actions/test_action_queries.cpp
    actions/test_action_repeat.cpp
    actions/test_action_scenes.cpp
    actions/test_action_set.cpp
    actions/test_bindings_format.cpp
    actions/test_bindings_io.cpp
    actions/test_input_view.cpp
    actions/test_source_eval.cpp
    actions/test_source_names.cpp
    animation/test_animator_clock.cpp
    animation/test_animator_draw.cpp
    animation/test_animator_notices.cpp
    animation/test_animator_play.cpp
    animation/test_animator_reentrancy.cpp
    animation/test_animator_sequences.cpp
    animation/test_animator_states.cpp
    animation/sprite_file.cpp
    animation/test_sprite_alloc.cpp
    animation/test_sprite_aseprite.cpp
    animation/test_sprite_clips.cpp
    animation/test_sprite_frames.cpp
    animation/test_sprite_layout.cpp
    animation/test_sprite_library.cpp
    animation/test_sprite_load.cpp
    animation/test_sprite_lifetime.cpp
    animation/test_sprite_markers.cpp
    animation/test_sprite_sequence.cpp
    animation/test_sprite_states.cpp
    animation/test_sprite_validate.cpp
    app/test_data_folder.cpp
    app/test_frame_loop.cpp
    app/test_input_hooks.cpp
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
    camera/test_camera_apply.cpp
    camera/test_camera_convert.cpp
    camera/test_camera_debug.cpp
    camera/test_camera_dead_zone.cpp
    camera/test_camera_follow.cpp
    camera/test_camera_layouts.cpp
    camera/test_camera_limits.cpp
    camera/test_camera_misuse.cpp
    camera/test_camera_noise.cpp
    camera/test_camera_shake.cpp
    camera/test_camera_smoothing.cpp
    camera/test_camera_snap.cpp
    camera/test_camera_update.cpp
    camera/test_camera_value.cpp
    camera/test_camera_zoom.cpp
    collision/test_aabb.cpp
    collision/test_box_cast.cpp
    collision/test_box_keys.cpp
    collision/test_box_view.cpp
    collision/test_cells_crossed.cpp
    collision/test_cells_touching.cpp
    collision/test_contacts.cpp
    collision/test_exact.cpp
    collision/test_grid_walk.cpp
    collision/test_grid_walk_lattice.cpp
    collision/test_grid.cpp
    collision/test_grid_move.cpp
    collision/test_hit_list.cpp
    collision/test_layers.cpp
    collision/test_one_way.cpp
    collision/test_point_queries.cpp
    collision/test_query_misuse.cpp
    collision/test_ray_box.cpp
    collision/test_raycast_all_grid.cpp
    collision/test_raycast_boxes.cpp
    collision/test_raycast_boxes_scan.cpp
    collision/test_raycast_grid.cpp
    collision/test_raycast_world.cpp
    core/test_crc32.cpp
    core/test_name_hash.cpp
    crash/test_assert.cpp
    data_assets/data_file.cpp
    data_assets/data_fixture.cpp
    data_assets/index_file.cpp
    data_assets/test_data_alloc.cpp
    data_assets/test_data_apply.cpp
    data_assets/test_data_index.cpp
    data_assets/test_data_layout.cpp
    data_assets/test_data_list.cpp
    data_assets/test_data_load.cpp
    data_assets/test_data_validate.cpp
    data_assets/test_enum_names.cpp
    data_assets/test_fingerprint.cpp
    data_assets/test_index_lookup.cpp
    data_assets/test_index_validate.cpp
    data_assets/test_refs.cpp
    data_assets/test_round_trip.cpp
    data_assets/test_schema_check.cpp
    data_assets/test_schema_declare.cpp
    data_assets/test_schema_registry.cpp
    data_assets/test_schema_write.cpp
    data_assets/test_string_list.cpp
    crash/test_report.cpp
    debug/test_debug_list.cpp
    debug/test_debug_pass.cpp
    debug/test_overlay.cpp
    debug/test_profile.cpp
    entities/test_block_layout.cpp
    entities/test_components.cpp
    entities/test_spawn.cpp
    entities/test_storage.cpp
    entities/test_tags.cpp
    entities/test_world_create.cpp
    entities/test_destroy_hooks.cpp
    entities/test_each.cpp
    entities/test_find.cpp
    entities/test_interop.cpp
    entities/test_flush_clear.cpp
    entities/test_world_misuse.cpp
    entities/test_world_scene.cpp
    entities/test_world_model.cpp
    entities/test_world_module.cpp
    entities/test_world_reserve.cpp
    entities/test_world_reserve_many.cpp
    entities/world_fixture.cpp
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
    fuzz/test_fuzz_sprite.cpp
    fuzz/test_fuzz_prefab_ops.cpp
    fuzz/test_fuzz_hierarchy.cpp
    fuzz/test_fuzz_world_ops.cpp
    gfx/test_camera_zoom_clamp.cpp
    gfx/test_draw_calls.cpp
    gfx/test_draw_list.cpp
    gfx/test_draw_list_zoom.cpp
    gfx/test_draw_transform.cpp
    gfx/test_texture_rules.cpp
    gfx/test_texture_table.cpp
    gfx/test_tile_batches.cpp
    gfx/test_tile_submit.cpp
    gfx/test_textures.cpp
    gfx/test_tile_chunks.cpp
    gfx/test_images.cpp
    gfx/test_zoom1_regression.cpp
    hierarchy/test_attach.cpp
    hierarchy/test_camera_follow.cpp
    hierarchy/test_cascade.cpp
    hierarchy/test_debug_draw.cpp
    hierarchy/test_each_descendant.cpp
    hierarchy/test_hierarchy_misuse.cpp
    hierarchy/test_keep_world.cpp
    hierarchy/test_node_compute.cpp
    hierarchy/test_node.cpp
    hierarchy/test_propagate.cpp
    hierarchy/test_queries.cpp
    hierarchy/test_remove_node.cpp
    hierarchy/test_scene_binding.cpp
    hierarchy/test_world_now.cpp
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
    pool/test_handle.cpp
    pool/test_pool.cpp
    pool/test_pool_iteration.cpp
    pool/test_pool_runtime.cpp
    pool/test_pool_scene.cpp
    pool/test_slot_bits.cpp
    prefabs/test_add.cpp
    prefabs/test_all_or_nothing.cpp
    prefabs/test_data_values.cpp
    prefabs/test_library_create.cpp
    prefabs/test_library_layout.cpp
    prefabs/test_map_names.cpp
    prefabs/test_map_objects.cpp
    prefabs/test_names.cpp
    prefabs/test_overrides.cpp
    prefabs/test_placement.cpp
    prefabs/test_prefab_misuse.cpp
    prefabs/test_queries.cpp
    prefabs/test_spawn.cpp
    prefabs/test_spawn_hooks.cpp
    prefabs/test_spawn_under.cpp
    prefabs/test_children.cpp
    prefabs/test_map_trees.cpp
    prefabs/test_tree_all_or_nothing.cpp
    prefabs/test_tree_spawn.cpp
    prefabs/test_variants.cpp
    remote/test_remote_input.cpp
    remote/test_remote_reader.cpp
    remote/test_remote_records.cpp
    remote/test_remote_schedule.cpp
    remote/test_remote_wire.cpp
    remote/test_remote_slider.cpp
    result/test_result.cpp
    scene/test_run.cpp
    scene/test_scene_attachments.cpp
    scene/test_scene_stack.cpp
    system/test_system.cpp
    system/test_stereo_slider.cpp
    text/test_builtin_font.cpp
    tilemap/test_code_vs_file.cpp
    tilemap/test_collision_grid_regression.cpp
    tilemap/test_map_animations.cpp
    tilemap/test_map_bounds.cpp
    tilemap/test_filtered_grid.cpp
    tilemap/test_map_build.cpp
    tilemap/test_map_collision.cpp
    tilemap/test_map_collision_layers.cpp
    tilemap/test_map_draw.cpp
    tilemap/test_map_draw_zoom.cpp
    tilemap/test_map_edit_after_load.cpp
    tilemap/test_map_layout.cpp
    tilemap/test_map_load.cpp
    tilemap/test_map_load_collision_layers.cpp
    tilemap/test_map_objects.cpp
    tilemap/test_map_properties.cpp
    tilemap/map_file.cpp
    tilemap/test_map_tiles.cpp
    tilemap/test_map_validate.cpp
    tilemap/test_map_validate_animations.cpp
    tilemap/test_map_validate_content.cpp
    tilemap/test_tile_animation.cpp
    tilemap/test_tile_expand.cpp
    tilemap/test_tile_expand_zoom.cpp
    tilemap/test_tilemap_alloc.cpp
    tilemap/test_visible_range.cpp
    tilemap/test_visible_range_zoom.cpp
    tilemap/test_zoom_edges.cpp
    simulator/test_audio_mixer_pc.cpp
    simulator/test_remote_address.cpp
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
    time/test_frame_time.cpp
    transform/test_accuracy.cpp
    transform/test_affine.cpp
    transform/test_bounds.cpp
    transform/test_compose.cpp
    transform/test_relative.cpp
    transform/test_sin_cos.cpp
    transform/test_sin_cos_flat.cpp
    tween/test_ease.cpp
    tween/test_ease_golden.cpp
    tween/test_ease_math.cpp
    tween/test_frame_clock.cpp
    tween/test_sequence.cpp
    tween/test_sequence_loops.cpp
    tween/test_timer.cpp
    tween/test_tween_alloc.cpp
    tween/test_tween_finish.cpp
    tween/test_tween_group.cpp
    tween/test_tween_handles.cpp
    tween/test_tween_loops.cpp
    tween/test_tween_misuse.cpp
    tween/test_tween_reentrancy.cpp
    tween/test_tween_scene.cpp
    tween/test_tween_set.cpp
    tween/test_tween_start.cpp
    tween/test_tween_timers.cpp
    tween/test_tween_values.cpp
)
target_include_directories(nnh_tests PRIVATE ${PROJECT_SOURCE_DIR}/engine/core/src)
# The remote session in the frame (spec 013) exists only in Debug and Development builds; Release compiles it out,
# so its behaviour tests are left out there (the wire, reader, schedule and merge stay: they are pure).
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    target_sources(nnh_tests PRIVATE remote/test_remote_frames.cpp remote/test_remote_screenshot.cpp
                                     remote/test_remote_session.cpp remote/test_remote_exclusive.cpp
                                     actions/test_action_input_paths.cpp)
    # Shutdown hooks run before the leak report, which Release doesn't have (spec 014 T3.3).
    target_sources(nnh_tests PRIVATE app/test_shutdown_hooks.cpp)
endif()
# Sound fixtures (tests/data/sounds, made by make_sounds.py) are read from the source tree.
target_compile_definitions(nnh_tests PRIVATE NNH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_link_libraries(nnh_tests PRIVATE nnh_test_framework nnh_fuzz_targets nnh::core nnh::platform_fake)
# MinGW: the C++ runtime linked in. Git Bash puts its own /mingw64/bin (a libstdc++-6.dll built against msvcrt.dll) first
# on PATH; a string that DLL allocates and an inlined Release destructor frees through the UCRT corrupts the heap
# (spec 029 stage 6, the localization fixtures). Tests only: games use no C++ runtime allocation.
if(MINGW)
    target_link_options(nnh_tests PRIVATE -static-libstdc++ -static-libgcc)
endif()
# The tests' data schema (spec 014), declared like a game's: compiled into nnh_tests and into its schema program
# nnh_tests_data_schema (cmake/nnh_data.cmake), whose description converts the fixtures of data/data_assets/game
# through the real asset pipeline (target nnh_test_data) for the code-vs-file test (T7.5, test_round_trip.cpp).
nnh_add_data_schema(nnh_tests data_assets/test_types.cpp)
get_target_property(NNH_TESTS_DATA_SCHEMA nnh_tests NNH_DATA_SCHEMA_FILE)
add_subdirectory(data/data_assets/game ${CMAKE_CURRENT_BINARY_DIR}/data_assets_game)
add_dependencies(nnh_tests nnh_test_data)
target_compile_definitions(nnh_tests PRIVATE NNH_TEST_DATA_GAME_DIR="${CMAKE_CURRENT_BINARY_DIR}/data_assets_game/assets")
# The schema program writes exactly the fixtures' committed description, and only when it changes (T7.2, T6.3).
add_test(NAME data_schema_program_writes_the_fixtures_schema
    COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:nnh_tests_data_schema>
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/data_assets_schema_check.nnhschema
            -DCOMMITTED=${CMAKE_CURRENT_SOURCE_DIR}/data/data_assets/schema.nnhschema -DROOT=${PROJECT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/data/data_assets/check_schema_program.cmake)
add_test(NAME data_schema_program_explains_its_usage COMMAND nnh_tests_data_schema)
set_tests_properties(data_schema_program_explains_its_usage PROPERTIES
    PASS_REGULAR_EXPRESSION "usage: <game>_data_schema <out[.]nnhschema>")
# Two schema sources declaring one key: the program refuses, naming both (T7.2, FR-1).
add_executable(nnh_test_dup_key_schema data_assets/schema_dup_a.cpp data_assets/schema_dup_b.cpp)
target_link_libraries(nnh_test_dup_key_schema PRIVATE nnh_data_schema_support)
add_test(NAME data_schema_program_refuses_a_key_used_twice
         COMMAND nnh_test_dup_key_schema ${CMAKE_CURRENT_BINARY_DIR}/data_assets_dup.nnhschema)
set_tests_properties(data_schema_program_refuses_a_key_used_twice PROPERTIES WILL_FAIL TRUE)
add_test(NAME data_schema_program_names_both_sources_of_a_key
         COMMAND nnh_test_dup_key_schema ${CMAKE_CURRENT_BINARY_DIR}/data_assets_dup.nnhschema)
set_tests_properties(data_schema_program_names_both_sources_of_a_key PROPERTIES PASS_REGULAR_EXPRESSION
    "schema_dup_[ab][.]cpp: types dup_[ab]::[A-Za-z]+ and dup_[ab]::[A-Za-z]+ both use the key 'thing' [(]also in [^)]*schema_dup_[ab][.]cpp[)]")
# A game whose data file is broken fails to build with `file:line: reason` (T7.6, AC-3, FR-9).
add_subdirectory(data/data_assets/broken_game ${CMAKE_CURRENT_BINARY_DIR}/data_assets_broken_game)
foreach(_name fails explains)
    add_test(NAME data_build_${_name}_with_file_line
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target nnh_test_broken_data_assets)
    # One Ninja at a time in this build folder (ctest -j).
    set_tests_properties(data_build_${_name}_with_file_line PROPERTIES RESOURCE_LOCK nnh_build_folder)
endforeach()
set_tests_properties(data_build_fails_with_file_line PROPERTIES WILL_FAIL TRUE)
set_tests_properties(data_build_explains_with_file_line PROPERTIES PASS_REGULAR_EXPRESSION
    "assets/enemies/goblin[.]enemy:3: unknown field 'lvel' [(]did you mean 'level'[?][)]")
# The PC simulator's window layout and settings are pure (SDL-free) and tested here (spec 006).
set(_pc_src ${PROJECT_SOURCE_DIR}/engine/platforms/pc/src)
target_sources(nnh_tests PRIVATE ${_pc_src}/audio_mixer_pc.cpp ${_pc_src}/remote_address_pc.cpp
                                 ${_pc_src}/sim_layout.cpp ${_pc_src}/sim_settings.cpp)
target_include_directories(nnh_tests PRIVATE ${_pc_src})
# The 3DS homebrew loader's argument block is pure code too (spec 011 FR-4).
set(_3ds_src ${PROJECT_SOURCE_DIR}/engine/platforms/3ds/src)
target_sources(nnh_tests PRIVATE platform_3ds/test_hbldr_args.cpp ${_3ds_src}/hbldr_args.cpp
                                 platform_3ds/test_pass_cache3d.cpp) # the 3D pass's state cache (spec 026, header-only)
target_include_directories(nnh_tests PRIVATE ${_3ds_src})
# The platformer's automatic gate (spec 013 FR-6) and its level logic (spec 012 stage 7: camera, coins, replay,
# 60 Hz meter, player physics) are pure files of the example, tested like core logic.
set(_platformer ${PROJECT_SOURCE_DIR}/examples/platformer)
target_sources(nnh_tests PRIVATE
    platformer/test_barrels.cpp
    platformer/test_level_camera.cpp
    platformer/test_level_prefabs.cpp
    platformer/test_coins.cpp
    platformer/test_frame_meter.cpp
    platformer/test_gate_run.cpp
    platformer/test_player.cpp
    platformer/test_player_sprite.cpp
    platformer/test_replay_run.cpp
    platformer/test_replay_script.cpp
    platformer/test_sparkles.cpp
    platformer/test_turrets.cpp
    ${_platformer}/level_camera.cpp
    ${_platformer}/coins.cpp
    ${_platformer}/level_prefabs.cpp
    ${_platformer}/level_world.cpp
    ${_platformer}/frame_meter.cpp
    ${_platformer}/gate_run.cpp
    ${_platformer}/player.cpp
    ${_platformer}/tuning.cpp
    ${_platformer}/player_sprite.cpp
    ${_platformer}/replay_run.cpp
    ${_platformer}/replay_script.cpp
    ${_platformer}/sparkles.cpp
    ${_platformer}/turrets.cpp)
# The replay played twice gives the same log (spec 012 T7.3, FR-10): the simulator's checksum lines must be the ones
# the console script expects (written from an earlier PC run). Release logs no info lines, so Debug/Development only.
if(TARGET platformer AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME platformer_replay_matches_script
             COMMAND ${CMAKE_COMMAND} -DGAME=$<TARGET_FILE:platformer> -DSCRIPT=${_platformer}/tests/level.nnhs
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/platformer/replay_matches_script.cmake)
    set_tests_properties(platformer_replay_matches_script PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;PLATFORMER_DEMO=replay;NNH_EXIT_AFTER_FRAMES=920"
        TIMEOUT 120)
endif()
# The animation example's self-check in the simulator (spec 016 AC-6): the verdict the console script waits for.
if(TARGET animation AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME animation_self_check_pc COMMAND animation)
    set_tests_properties(animation_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;ANIMATION_DEMO=check;NNH_EXIT_AFTER_FRAMES=610"
        PASS_REGULAR_EXPRESSION "animation: PASS" FAIL_REGULAR_EXPRESSION "animation: FAIL" TIMEOUT 120)
endif()
# The tweens example's self-check in the simulator (spec 015 AC-5): the same verdict the console script waits for.
if(TARGET tweens AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME tweens_self_check_pc COMMAND tweens)
    set_tests_properties(tweens_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;TWEENS_DEMO=check;NNH_EXIT_AFTER_FRAMES=610"
        PASS_REGULAR_EXPRESSION "tweens: PASS B44CA3F6" FAIL_REGULAR_EXPRESSION "tweens: FAIL" TIMEOUT 120)
endif()
# The camera example's self-check in the simulator (spec 018 AC-6): the verdict the console script waits for.
if(TARGET camera AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME camera_self_check_pc COMMAND camera)
    set_tests_properties(camera_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;CAMERA_DEMO=check;NNH_EXIT_AFTER_FRAMES=615"
        PASS_REGULAR_EXPRESSION "camera: PASS 5201E6CF" FAIL_REGULAR_EXPRESSION "camera: FAIL" TIMEOUT 120)
endif()
# Scene transitions (spec 023): the example's self-check in the simulator (AC-3), its console script and the
# platformer's door run played with nnh run --pc (AC-6, AC-7), as the maintainer plays them on the 3DS.
if(NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    set(_sim_env "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy")
    get_filename_component(_preset ${CMAKE_BINARY_DIR} NAME)
    if(TARGET transitions)
        add_test(NAME transitions_self_check_pc COMMAND transitions)
        set_tests_properties(transitions_self_check_pc PROPERTIES
            ENVIRONMENT "${_sim_env};TRANSITIONS_DEMO=check;NNH_EXIT_AFTER_FRAMES=1250"
            PASS_REGULAR_EXPRESSION "transitions: PASS BFE1D810" FAIL_REGULAR_EXPRESSION "transitions: FAIL" TIMEOUT 120)
        add_test(NAME transitions_check_script_pc
                 COMMAND $<TARGET_FILE:nnh> run transitions --pc --no-build --script examples/transitions/tests/check.nnhs
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        set_tests_properties(transitions_check_script_pc PROPERTIES ENVIRONMENT "${_sim_env};NNH_PRESET=${_preset}"
            PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 300
            RESOURCE_LOCK nnh_remote_port)
    endif()
    if(TARGET platformer)
        add_test(NAME platformer_doors_script_pc
                 COMMAND $<TARGET_FILE:nnh> run platformer --pc --no-build --script examples/platformer/tests/doors.nnhs
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        set_tests_properties(platformer_doors_script_pc PROPERTIES ENVIRONMENT "${_sim_env};NNH_PRESET=${_preset}"
            PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 300
            RESOURCE_LOCK nnh_remote_port)
    endif()
endif()
# The queries example's self-check in the simulator (spec 019 AC-6): the verdict the console script waits for.
if(TARGET queries AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME queries_self_check_pc COMMAND queries)
    set_tests_properties(queries_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;QUERIES_DEMO=check;NNH_EXIT_AFTER_FRAMES=90"
        PASS_REGULAR_EXPRESSION "queries: PASS 4ACAA177" FAIL_REGULAR_EXPRESSION "queries: FAIL" TIMEOUT 120)
endif()
# The saves example (spec 027 AC-2, AC-6): its golden copies equal tests/data/saves; its check mode and its console
# script played in the simulator (nnh run --pc), as the maintainer plays it on the 3DS.
if(TARGET saves AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME saves_self_check_pc COMMAND saves)
    set_tests_properties(saves_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;SAVES_DEMO=check;NNH_EXIT_AFTER_FRAMES=20"
        PASS_REGULAR_EXPRESSION "saves: PASS" FAIL_REGULAR_EXPRESSION "saves: FAIL" TIMEOUT 120
        RESOURCE_LOCK nnh_saves_data)
    get_filename_component(_preset ${CMAKE_BINARY_DIR} NAME)
    add_test(NAME saves_check_script_pc
             COMMAND $<TARGET_FILE:nnh> run saves --pc --no-build --script examples/saves/tests/check.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(saves_check_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 300
        RESOURCE_LOCK "nnh_remote_port;nnh_saves_data")
endif()
add_test(NAME saves_golden_copies_match
         COMMAND ${CMAKE_COMMAND} -DA=${CMAKE_CURRENT_SOURCE_DIR}/data/saves -DB=${PROJECT_SOURCE_DIR}/examples/saves/assets/golden
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/saves/compare_golden.cmake)
# The entities example's self-check in the simulator (spec 020 T7.4, AC-6): the verdict the console script waits for.
if(TARGET entities AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME entities_self_check_pc COMMAND entities)
    set_tests_properties(entities_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;ENTITIES_DEMO=check;NNH_EXIT_AFTER_FRAMES=70"
        PASS_REGULAR_EXPRESSION "entities: PASS C8FB9A41" FAIL_REGULAR_EXPRESSION "entities: FAIL" TIMEOUT 120)
endif()
# The particles example's self-check in the simulator (spec 024 T7.3, AC-6) and its console script played with --pc.
if(TARGET particles AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME particles_self_check_pc COMMAND particles)
    set_tests_properties(particles_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;PARTICLES_DEMO=check;NNH_EXIT_AFTER_FRAMES=380"
        PASS_REGULAR_EXPRESSION "particles: PASS C5EF1A84" FAIL_REGULAR_EXPRESSION "particles: FAIL|\\] E " TIMEOUT 120)
    get_filename_component(_preset ${CMAKE_BINARY_DIR} NAME)
    add_test(NAME particles_check_script_pc
             COMMAND $<TARGET_FILE:nnh> run particles --pc --no-build --script examples/particles/tests/check.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(particles_check_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 400
        RESOURCE_LOCK nnh_remote_port)
endif()
# The prefabs example's self-check in the simulator (spec 021 T7.4, AC-6): the verdict the console script waits for; and
# the console script itself played in the simulator (nnh run --pc), as the maintainer plays it on the 3DS.
if(TARGET prefabs AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME prefabs_self_check_pc COMMAND prefabs)
    set_tests_properties(prefabs_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;PREFABS_DEMO=check;NNH_EXIT_AFTER_FRAMES=60"
        PASS_REGULAR_EXPRESSION "prefabs: PASS 7BB7901E" FAIL_REGULAR_EXPRESSION "prefabs: FAIL" TIMEOUT 120)
    # (tools/nnh is configured after tests/: the target is named by a generator expression, not checked here.)
    get_filename_component(_preset ${CMAKE_BINARY_DIR} NAME)
    add_test(NAME prefabs_check_script_pc
             COMMAND $<TARGET_FILE:nnh> run prefabs --pc --no-build --script examples/prefabs/tests/check.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(prefabs_check_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 300
        RESOURCE_LOCK nnh_remote_port)
endif()
# The hierarchy example's self-check in the simulator (spec 022 T7.3, AC-6) and its console script played with --pc.
if(TARGET hierarchy AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME hierarchy_self_check_pc COMMAND hierarchy)
    set_tests_properties(hierarchy_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;HIERARCHY_DEMO=check;NNH_EXIT_AFTER_FRAMES=60"
        PASS_REGULAR_EXPRESSION "hierarchy: PASS 23DB5C36" FAIL_REGULAR_EXPRESSION "hierarchy: FAIL" TIMEOUT 120)
    get_filename_component(_preset ${CMAKE_BINARY_DIR} NAME)
    add_test(NAME hierarchy_check_script_pc
             COMMAND $<TARGET_FILE:nnh> run hierarchy --pc --no-build --script examples/hierarchy/tests/check.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(hierarchy_check_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 300
        RESOURCE_LOCK nnh_remote_port)
    # The same script on a console whose Circle Pad rests past the dead zone (an Old 3DS at raw (10, 14) of 156):
    # a script owns the controls, so it still passes, and the console warns once (spec 013 amendment 2026-11-24).
    add_test(NAME hierarchy_check_script_resting_pad_pc
             COMMAND $<TARGET_FILE:nnh> run hierarchy --pc --no-build --script examples/hierarchy/tests/check.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(hierarchy_check_script_resting_pad_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_preset};NNH_SIM_INPUT=circle=0.0641025641,0.0897435897"
        PASS_REGULAR_EXPRESSION "nnh: the console's own controls are ignored while the script runs: Circle Pad not at rest.*nnh: PASS"
        FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 300
        RESOURCE_LOCK nnh_remote_port)
endif()
# The data example's self-check in the simulator (spec 014 T8.3): the line the console script expects.
if(TARGET data AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME data_self_check_pc COMMAND data)
    set_tests_properties(data_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;DATA_DEMO=check;NNH_EXIT_AFTER_FRAMES=10"
        PASS_REGULAR_EXPRESSION "data check: PASS checksum 0xD5BD3020 [(]4 enemies, 3 items, 1 settings, index 595 bytes[)]"
        FAIL_REGULAR_EXPRESSION "data check: FAIL|\\] E " TIMEOUT 120)
endif()
# The pathfinding example's self-check in the simulator (spec 025 T7.3, AC-5) and its console script played with --pc.
if(TARGET pathfinding AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME pathfinding_self_check_pc COMMAND pathfinding)
    set_tests_properties(pathfinding_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;PATHFINDING_DEMO=check;NNH_EXIT_AFTER_FRAMES=80"
        PASS_REGULAR_EXPRESSION "pathfinding: PASS 1C0B5992" FAIL_REGULAR_EXPRESSION "pathfinding: FAIL|\\] E " TIMEOUT 120)
    get_filename_component(_preset ${CMAKE_BINARY_DIR} NAME)
    add_test(NAME pathfinding_check_script_pc
             COMMAND $<TARGET_FILE:nnh> run pathfinding --pc --no-build --script examples/pathfinding/tests/check.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(pathfinding_check_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 400
        RESOURCE_LOCK nnh_remote_port)
endif()
target_include_directories(nnh_tests PRIVATE ${PROJECT_SOURCE_DIR}/examples)
# The tweens example's self-check (spec 015 T6.1): its golden checksum must hold on every host compiler.
target_sources(nnh_tests PRIVATE tweens/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/tweens/self_check.cpp)
# The hot reload example's self-check fold (spec 031 T7.3): the same bits on every host compiler.
target_sources(nnh_tests PRIVATE reload_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/reload/self_check.cpp)
# The animation example's self-check (spec 016 T8.2): its golden checksum on every host compiler.
target_sources(nnh_tests PRIVATE animation_example/test_self_check.cpp
                                 ${PROJECT_SOURCE_DIR}/examples/animation/libraries.cpp
                                 ${PROJECT_SOURCE_DIR}/examples/animation/self_check.cpp)
# PC microbenchmark of an action set's evaluation (spec 017 console bench refinement): not a test, run by hand
# (build/pc-release/tests/nnh_actions_bench) for relative gains.
add_executable(nnh_actions_bench EXCLUDE_FROM_ALL actions/bench_actions.cpp)
target_include_directories(nnh_actions_bench PRIVATE ${PROJECT_SOURCE_DIR}/engine/core/src)
target_link_libraries(nnh_actions_bench PRIVATE nnh::core nnh::platform_fake)
# The camera example's self-check (spec 018 T6.2): its golden checksum on every host compiler.
target_sources(nnh_tests PRIVATE camera_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/camera/self_check.cpp)
# Scene transitions and the input block (spec 023): every test of the spec in one block (merge notes, plan 023).
target_sources(nnh_tests PRIVATE
    fuzz/test_fuzz_transition_geometry.cpp
    input/test_input_block.cpp
    input/test_input_block_api.cpp
    platformer/test_door_run.cpp
    ${PROJECT_SOURCE_DIR}/examples/platformer/door_run.cpp
    ${PROJECT_SOURCE_DIR}/examples/platformer/door_transition.cpp
    scene/test_scene_hook.cpp
    transition/test_custom_effect.cpp
    transition/test_effect_area.cpp
    transition/test_effect_fill.cpp
    transition/test_effect_iris.cpp
    transition/test_effect_query.cpp
    transition/test_geometry_properties.cpp
    transition/test_interruptions.cpp
    transition/test_loading.cpp
    transition/test_no_wrong_frame.cpp
    transition/test_notifications.cpp
    transition/test_overlay_order.cpp
    transition/test_plain_loop.cpp
    transition/test_requests.cpp
    transition/test_scene_updates.cpp
    transition/test_stack_change.cpp
    transition/test_timeline.cpp
    transition/test_transition_actions.cpp
    transition/test_transition_cost.cpp
    transition/test_transition_input.cpp
    transitions_example/test_self_check.cpp
    ${PROJECT_SOURCE_DIR}/examples/transitions/check_script.cpp
    ${PROJECT_SOURCE_DIR}/examples/transitions/check_steps.cpp)
target_sources(nnh_fuzz_targets PRIVATE fuzz/fuzz_transition_geometry.cpp)
if(NNH_BUILD_FUZZERS)
    add_executable(fuzz_transition_geometry fuzz/libfuzzer_transition_geometry.cpp)
    target_link_libraries(fuzz_transition_geometry PRIVATE nnh_fuzz_targets)
    target_compile_options(fuzz_transition_geometry PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_transition_geometry PRIVATE -fsanitize=fuzzer,address)
endif()
# The queries example's self-check (spec 019 T7.3, AC-2): its golden checksum on every host compiler.
target_sources(nnh_tests PRIVATE queries_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/queries/self_check.cpp)
# Structured saves (spec 027): the codec, evolution, migrations, files, power cuts, golden files, fuzz.
target_sources(nnh_tests PRIVATE
    platform_fake/test_power_cut_fake.cpp
    saves/test_backup.cpp
    saves/test_blob.cpp
    saves/test_bulk.cpp
    saves/test_declare.cpp
    saves/test_dump.cpp
    saves/test_edges.cpp
    saves/test_encode.cpp
    saves/test_encode_sequences.cpp
    saves/test_evolution.cpp
    saves/test_file.cpp
    saves/test_fit.cpp
    saves/test_golden.cpp
    saves/test_header.cpp
    saves/test_migrations.cpp
    saves/test_old_fields.cpp
    saves/test_power_cut.cpp
    saves/test_raw_unchanged.cpp
    saves/test_reference.cpp
    saves/test_round_trip.cpp
    saves/test_rows.cpp
    saves/test_save_cost.cpp
    saves/test_save_misuse.cpp
    saves/test_slots.cpp
    saves/test_summary.cpp
    saves/test_validate.cpp
    saves/test_versions.cpp
    saves/test_wire.cpp)
# The reference save (AC-2): the saves example's Progress and its history, compiled into the tests (its NNH_DATA_ENUM
# names are not: they would change spec 014's registry tests). make_saves_golden writes tests/data/saves by hand.
set(_saves_example_sources ${PROJECT_SOURCE_DIR}/examples/saves/progress_migrate.cpp
                           ${PROJECT_SOURCE_DIR}/examples/saves/progress_sum.cpp
                           ${PROJECT_SOURCE_DIR}/examples/saves/reference.cpp)
target_sources(nnh_tests PRIVATE ${_saves_example_sources})
add_executable(make_saves_golden EXCLUDE_FROM_ALL saves/make_saves_golden.cpp ${_saves_example_sources})
target_include_directories(make_saves_golden PRIVATE ${PROJECT_SOURCE_DIR}/engine/core/src ${PROJECT_SOURCE_DIR}/examples)
target_link_libraries(make_saves_golden PRIVATE nnh::core nnh::platform_fake)
# The structured-save fuzz target (AC-4): its own header (fuzz/fuzz_saves.hpp); a mini-fuzz in nnh_tests, and a
# libFuzzer executable with NNH_BUILD_FUZZERS (clang, Linux).
target_sources(nnh_fuzz_targets PRIVATE fuzz/fuzz_saves.cpp)
target_sources(nnh_tests PRIVATE fuzz/test_fuzz_saves.cpp)
if(NNH_BUILD_FUZZERS)
    add_executable(fuzz_saves fuzz/libfuzzer_saves.cpp)
    target_link_libraries(fuzz_saves PRIVATE nnh_fuzz_targets)
    target_compile_options(fuzz_saves PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_saves PRIVATE -fsanitize=fuzzer,address)
endif()
# Save declarations (spec 027 T2.3, AC-1): a correct one compiles with -Werror; each mistake fails to compile AND says
# what is wrong. `~` separates alternatives (any one passes): GCC's wording, then clang's.
if(NOT MSVC)
    set(_save_check ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only -Wall -Wextra -I${PROJECT_SOURCE_DIR}/engine/core/include)
    set(_save_cases ${CMAKE_CURRENT_SOURCE_DIR}/saves/compile_fail)
    add_test(NAME saves_declaration_accepts_good COMMAND ${_save_check} -Werror ${_save_cases}/good.cpp)
    foreach(case "unknown_member|damgae[^ ]* is not a member~no member named .damgae."
                 "pointer|saved fields can't be pointers or references"
                 "std_string|saved fields must be trivially copyable"
                 "union|unions aren't saved field kinds"
                 "char|character types aren't saved field kinds"
                 "const_member|saved fields can't be const or volatile"
                 "id_zero|save_error_field_id_must_be_1_to_65535"
                 "id_65536|save_error_field_id_must_be_1_to_65535"
                 "duplicate_id|save_error_field_id_used_twice"
                 "retired_used|save_error_retired_id_used_by_a_field"
                 "retired_twice|save_error_retired_id_listed_twice"
                 "nested_undeclared|needs its own NNH_SAVE_STRUCT"
                 "too_many_fields|save_error_more_than_128_fields"
                 "too_deep|save_error_nesting_deeper_than_8_levels"
                 "root_too_big|a save type can be at most 64 KiB"
                 "array_too_big|an array in a save holds at most 65535 items"
                 "max_size_too_big|save_error_max_encoded_size_over_256_KiB"
                 "bad_key_upper|save_error_bad_key_use_1_to_16"
                 "bad_key_long|save_error_bad_key_use_1_to_16"
                 "bad_key_empty|save_error_bad_key_use_1_to_16"
                 "version_zero|save_error_version_must_be_at_least_1"
                 "version_twice|save_error_version_given_twice"
                 "step_at_current|save_error_step_from_a_version_not_below_the_current_one"
                 "two_steps|save_error_two_steps_for_one_version"
                 "step_gap|save_error_missing_migration_step"
                 "version_in_struct|save_error_NNH_SAVE_VERSION_or_NNH_SAVE_STEP_in_a_struct_or_summary"
                 "summary_stored|only an NNH_SAVE_TYPE can be encoded or stored"
                 "undeclared_stored|this type has no save declaration")
        string(REPLACE "|" ";" case "${case}")
        list(GET case 0 name)
        list(GET case 1 expected)
        string(REPLACE "~" ";" expected "${expected}")
        add_test(NAME saves_declaration_rejects_${name} COMMAND ${_save_check} ${_save_cases}/${name}.cpp)
        set_tests_properties(saves_declaration_rejects_${name} PROPERTIES WILL_FAIL TRUE)
        add_test(NAME saves_declaration_explains_${name} COMMAND ${_save_check} ${_save_cases}/${name}.cpp)
        set_tests_properties(saves_declaration_explains_${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
    endforeach()
endif()
# UI widgets (spec 028): every test of the spec in one block.
target_sources(nnh_tests PRIVATE
    ui/test_actions_adapter.cpp
    ui/test_anchor.cpp
    ui/test_box_math.cpp
    ui/test_callbacks.cpp
    ui/test_cancel.cpp
    ui/test_create.cpp
    ui/test_cursor.cpp
    ui/test_debug.cpp
    ui/test_draw.cpp
    ui/test_draw_equivalence.cpp
    ui/test_draw_widgets.cpp
    ui/test_events.cpp
    ui/test_focus.cpp
    ui/test_geometric.cpp
    ui/test_grid.cpp
    ui/test_input_custom.cpp
    ui/test_input_raw.cpp
    ui/test_keys.cpp
    ui/test_layout_anchors.cpp
    ui/test_layout_boxes.cpp
    ui/test_layout_dirty.cpp
    ui/test_list.cpp
    ui/test_min_size.cpp
    ui/test_modal.cpp
    ui/test_navigate.cpp
    ui/test_nine_slice.cpp
    ui/test_no_leak.cpp
    ui/test_number.cpp
    ui/test_prompt.cpp
    ui/test_remembered_focus.cpp
    ui/test_repeat.cpp
    ui/test_results.cpp
    ui/test_reveal.cpp
    ui/test_scene_binding.cpp
    ui/test_scroll_box.cpp
    ui/test_style.cpp
    ui/test_text_seam.cpp
    ui/test_text_sources.cpp
    ui/test_touch.cpp
    ui/test_tree.cpp
    ui/test_ui_cost.cpp
    ui/test_ui_misuse.cpp
    ui/test_ui_random_ops.cpp
    ui/test_values.cpp)
# The UI example's self-check (spec 028 T7.1, AC-2): its golden checksums on every host compiler.
target_sources(nnh_tests PRIVATE ui_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/ui/self_check.cpp
                                 ${PROJECT_SOURCE_DIR}/examples/ui/self_check_script.cpp
                                 ui_example/test_hand_options.cpp ${PROJECT_SOURCE_DIR}/examples/ui/hand_options.cpp)
# The UI example in the simulator (spec 028 T7.4, T7.6): its self-check, and its console script played with --pc.
if(TARGET ui AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME ui_self_check_pc COMMAND ui)
    set_tests_properties(ui_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;UI_DEMO=check;NNH_EXIT_AFTER_FRAMES=420"
        PASS_REGULAR_EXPRESSION "ui: PASS 30400DA6" FAIL_REGULAR_EXPRESSION "ui: FAIL|\] E " TIMEOUT 120)
    get_filename_component(_preset ${CMAKE_BINARY_DIR} NAME)
    add_test(NAME ui_check_script_pc
             COMMAND $<TARGET_FILE:nnh> run ui --pc --no-build --script examples/ui/tests/check.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(ui_check_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 400
        RESOURCE_LOCK nnh_remote_port)
endif()
# Spec 028 T6.1: the style structs are data-loadable (compile only: linking them would add types to the registry).
if(NOT MSVC)
    add_test(NAME ui_style_structs_are_data_loadable
             COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only -Wall -Wextra -Werror
                     -I${PROJECT_SOURCE_DIR}/engine/core/include ${CMAKE_CURRENT_SOURCE_DIR}/ui/data_check/style_types.cpp)
endif()
# Game fonts and localization (spec 029): every test of the spec in one block.
# The converted font fixtures: the same bytes on every compiler and host (T4.9, AC-2).
add_test(NAME font_fixtures_golden
         COMMAND ${CMAKE_COMMAND} -DASSETC=$<TARGET_FILE:nnh_assetc> -DFONTS=${CMAKE_CURRENT_SOURCE_DIR}/data/fonts
                 -DOUT=${CMAKE_CURRENT_BINARY_DIR}/font_fixtures -P ${CMAKE_CURRENT_SOURCE_DIR}/data/fonts/golden.cmake)
set_tests_properties(font_fixtures_golden PROPERTIES PASS_REGULAR_EXPRESSION "font fixtures golden: PASS")
# Routing (T4.9): the example's asset index has the converted fonts, the tables, the manifest and the licence, and none
# of the sources (.font, .fnt, BMFont pages, .otf) nor the string files; editing one language's strings reruns only its
# own table step (T5.7: the per-language step depends on its files and the base's).
if(TARGET localization_assets)
    add_test(NAME font_routing
             COMMAND ${CMAKE_COMMAND} -DINDEX=${CMAKE_BINARY_DIR}/examples/localization/assets/.nnh_assets
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/data/fonts/routing.cmake)
    set_tests_properties(font_routing PROPERTIES PASS_REGULAR_EXPRESSION "font routing: PASS")
    if(CMAKE_GENERATOR MATCHES "Ninja")
        add_test(NAME strings_incremental
                 COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_BINARY_DIR} -t query
                         examples/localization/assets/strings/en.nnhstr)
        set_tests_properties(strings_incremental PROPERTIES
            PASS_REGULAR_EXPRESSION "en[.]strings" FAIL_REGULAR_EXPRESSION "de[.]strings|ja[.]strings")
    endif()
endif()
# Declarations of LANGUAGES (T5.7): a fixture project configured once per case; each refusal names the fix.
foreach(_case "missing|language de has no strings/de.strings"
              "unlisted|strings/fr.strings belongs to fr, which is not in"
              "bad_code|\"PT_br\" is not a language code"
              "both_forms|de has both strings/de.strings and strings/de/: keep"
              "bad_base|BASE ja is not in LANGUAGES"
              "no_languages|string files but no LANGUAGES")
    string(REPLACE "|" ";" _case "${_case}")
    list(GET _case 0 _name)
    list(GET _case 1 _expected)
    add_test(NAME strings_declaration_${_name}
             COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR}/data/strings_decl
                     -B ${CMAKE_CURRENT_BINARY_DIR}/strings_decl_${_name} -DCASE=${_name}
                     -DNNH_ROOT=${PROJECT_SOURCE_DIR})
    set_tests_properties(strings_declaration_${_name} PROPERTIES PASS_REGULAR_EXPRESSION "${_expected}")
endforeach()
add_test(NAME strings_declaration_good
         COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR}/data/strings_decl
                 -B ${CMAKE_CURRENT_BINARY_DIR}/strings_decl_good -DCASE=good -DNNH_ROOT=${PROJECT_SOURCE_DIR})
set_tests_properties(strings_declaration_good PROPERTIES PASS_REGULAR_EXPRESSION "strings declaration: [^\n]*en=[^\n]*de=")
# The localization example's self-check on the fake platform, over the example's own converted assets (T7.2).
if(TARGET localization_assets)
    target_sources(nnh_tests PRIVATE loc_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/localization/self_check.cpp)
    target_compile_definitions(nnh_tests PRIVATE
        NNH_LOC_EXAMPLE_ASSETS="${CMAKE_BINARY_DIR}/examples/localization/assets")
    add_dependencies(nnh_tests localization_assets)
endif()
# The localization example in the simulator (T7.4, T7.6): its self-check (every language switched like a player would),
# and its console script played with --pc.
if(TARGET localization AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME loc_self_check_pc COMMAND localization)
    set_tests_properties(loc_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;LOC_DEMO=check;NNH_EXIT_AFTER_FRAMES=60"
        PASS_REGULAR_EXPRESSION "loc: PASS 21809981" FAIL_REGULAR_EXPRESSION "loc: FAIL|\] E " TIMEOUT 120)
    get_filename_component(_loc_preset ${CMAKE_BINARY_DIR} NAME)
    # the script starts from "follow the system language": a saved choice left by an earlier, failed run is removed
    add_test(NAME loc_reset_saved_language
             COMMAND ${CMAKE_COMMAND} -E rm -f
                     ${CMAKE_BINARY_DIR}/examples/localization/data/nnhge-localization/localization.sav
                     ${CMAKE_BINARY_DIR}/examples/localization/data/nnhge-localization/localization.sav.bak)
    set_tests_properties(loc_reset_saved_language PROPERTIES FIXTURES_SETUP loc_saved_language)
    add_test(NAME loc_check_script_pc
             COMMAND $<TARGET_FILE:nnh> run localization --pc --no-build --script examples/localization/tests/check.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(loc_check_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_loc_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 400
        RESOURCE_LOCK nnh_remote_port FIXTURES_REQUIRED loc_saved_language)
endif()
target_sources(nnh_fuzz_targets PRIVATE fuzz/fuzz_font.cpp fuzz/fuzz_loc.cpp)
if(NNH_BUILD_FUZZERS)
    add_executable(fuzz_font_validate fuzz/libfuzzer_font_validate.cpp)
    target_link_libraries(fuzz_font_validate PRIVATE nnh_fuzz_targets)
    target_compile_options(fuzz_font_validate PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_font_validate PRIVATE -fsanitize=fuzzer,address)
    foreach(_loc_fuzz loc_table loc_manifest)
        add_executable(fuzz_${_loc_fuzz} fuzz/libfuzzer_${_loc_fuzz}.cpp)
        target_link_libraries(fuzz_${_loc_fuzz} PRIVATE nnh_fuzz_targets)
        target_compile_options(fuzz_${_loc_fuzz} PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_${_loc_fuzz} PRIVATE -fsanitize=fuzzer,address)
    endforeach()
endif()
target_sources(nnh_tests PRIVATE
    font/test_font_cost.cpp
    font/test_font_debug.cpp
    font/test_font_format.cpp
    font/test_font_load.cpp
    font/test_font_lookup.cpp
    font/test_font_misuse.cpp
    font/test_font_table.cpp
    font/test_font_validate.cpp
    font/test_page_codec.cpp
    fuzz/test_fuzz_font.cpp
    fuzz/test_fuzz_loc.cpp
    loc/test_loc_cost.cpp
    loc/test_loc_docs_snippets.cpp
    loc/test_loc_fonts.cpp
    loc/test_loc_format.cpp
    loc/test_loc_lookup.cpp
    loc/test_loc_number.cpp
    loc/test_loc_random_ops.cpp
    loc/test_loc_start.cpp
    loc/test_loc_switch.cpp
    loc/test_loc_ui.cpp
    loc/test_loc_validate.cpp
    loc/test_plural_rules.cpp
    platform_fake/test_texture_alpha_fake.cpp
    remote/test_remote_language.cpp
    system/test_language_tag.cpp
    system/test_system_language.cpp
    text/test_text_runs.cpp
    text/test_text_wrap.cpp
    ui/reference/ui_wrap_cells.cpp
    ui/test_ui_fonts.cpp
    ui/test_ui_seam_equivalence.cpp
    text/test_break_class.cpp
    text/test_measure_chars.cpp
    text/test_text_builtin_unchanged.cpp
    text/test_text_draw_fonts.cpp
    text/test_text_measure_box.cpp
    text/test_text_missing.cpp)
# The entities example's self-check (spec 020 T5.4, AC-2): its golden checksum on every host compiler.
target_sources(nnh_tests PRIVATE entities_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/entities/self_check.cpp)
# Particles (spec 024): unit tests, the shared helpers' tests.
target_sources(nnh_tests PRIVATE
    camera/test_mix32.cpp
    gfx/test_sprite_place.cpp
    particles/test_appearance.cpp
    particles/test_birth_golden.cpp
    particles/test_bursts.cpp
    particles/test_clock_edges.cpp
    particles/test_create.cpp
    particles/test_cull_reach.cpp
    particles/test_desc_check.cpp
    particles/test_directions.cpp
    particles/test_draw.cpp
    particles/test_expand.cpp
    particles/test_frames.cpp
    particles/test_lifecycle.cpp
    particles/test_motion.cpp
    particles/test_order_full.cpp
    particles/test_particle_budget.cpp
    particles/test_particle_cost.cpp
    particles/test_particle_misuse.cpp
    particles/test_header_alone.cpp
    particles/test_prewarm.cpp
    particles/test_queries.cpp
    particles/test_random.cpp
    particles/test_rate.cpp
    particles/test_scene_binding.cpp
    particles/test_set_desc.cpp
    particles/test_shapes.cpp
    particles/test_space.cpp
    particles/test_subframe.cpp
    fuzz/test_fuzz_particle_ops.cpp
    particles_example/test_self_check.cpp
    platformer/test_level_effects.cpp
    ${PROJECT_SOURCE_DIR}/examples/particles/self_check.cpp
    ${_platformer}/level_effects.cpp
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${PROJECT_SOURCE_DIR}/examples/particles/self_check.cpp PROPERTIES
                                COMPILE_OPTIONS -ffp-contract=off)
endif()
target_sources(nnh_fuzz_targets PRIVATE fuzz/fuzz_particle_ops.cpp)
if(NNH_BUILD_FUZZERS)
    add_executable(fuzz_particle_ops fuzz/libfuzzer_particle_ops.cpp)
    target_link_libraries(fuzz_particle_ops PRIVATE nnh_fuzz_targets)
    target_compile_options(fuzz_particle_ops PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_particle_ops PRIVATE -fsanitize=fuzzer,address)
endif()
# The prefabs example's self-check (spec 021 T5.4, AC-2): prefab spawns against hand-written ones, golden checksum.
set(_prefabs_example ${PROJECT_SOURCE_DIR}/examples/prefabs)
# (Its sources are compiled into nnh_fuzz_targets, which nnh_tests links: the fuzz target drives the same operations.)
target_sources(nnh_tests PRIVATE prefabs_example/test_self_check.cpp)
# The hierarchy example's self-check (spec 022 T5.5, AC-2): prefab hierarchies against hand-written ones, golden checksum.
# (Its sources are compiled into nnh_fuzz_targets too, for the fuzz target.)
target_sources(nnh_tests PRIVATE hierarchy_example/test_self_check.cpp)
# The actions example's self-check and log lines (spec 017 T6.4).
target_sources(nnh_tests PRIVATE actions_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/actions/action_log.cpp
                                 ${PROJECT_SOURCE_DIR}/examples/actions/self_check.cpp)
# Pathfinding on grids (spec 025): unit tests, references, the example's self-check, the fuzz target.
target_sources(nnh_tests PRIVATE
    pathfinding/path_reference.cpp
    pathfinding/test_field_build.cpp
    pathfinding/test_field_next.cpp
    pathfinding/test_field_slices.cpp
    pathfinding/test_find_basic.cpp
    pathfinding/test_find_costs.cpp
    pathfinding/test_follower.cpp
    pathfinding/test_footprint.cpp
    pathfinding/test_inspect.cpp
    pathfinding/test_limits_partial.cpp
    pathfinding/test_neighbors.cpp
    pathfinding/test_open_heap.cpp
    pathfinding/test_path_cost.cpp
    pathfinding/test_path_map_create.cpp
    pathfinding/test_path_map_edit.cpp
    pathfinding/test_path_map_sync.cpp
    pathfinding/test_path_misuse.cpp
    pathfinding/test_path_points.cpp
    pathfinding/test_reference_random.cpp
    pathfinding/test_reference_self.cpp
    pathfinding/test_searcher_reuse.cpp
    pathfinding/test_slicing.cpp
    pathfinding/test_smooth.cpp
    pathfinding/test_smooth_walk.cpp
    pathfinding/test_tie_order.cpp
    pathfinding_example/test_self_check.cpp
    fuzz/test_fuzz_path_ops.cpp
    ${PROJECT_SOURCE_DIR}/examples/pathfinding/check_maps.cpp
    ${PROJECT_SOURCE_DIR}/examples/pathfinding/self_check.cpp
)
target_sources(nnh_fuzz_targets PRIVATE fuzz/fuzz_path_ops.cpp)
if(NNH_BUILD_FUZZERS)
    add_executable(fuzz_path_ops fuzz/libfuzzer_path_ops.cpp)
    target_link_libraries(fuzz_path_ops PRIVATE nnh_fuzz_targets)
    target_compile_options(fuzz_path_ops PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_path_ops PRIVATE -fsanitize=fuzzer,address)
endif()
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
# PC only: tiles read back pixel by pixel from the screen's render target (spec 012). SDL3::SDL3 is imported per
# directory, so it is found again here (the download is reused).
include(nnh_sdl3)
target_sources(nnh_conformance_pc PRIVATE platform_conformance/test_draw_tiles_pc.cpp)
target_include_directories(nnh_conformance_pc PRIVATE ${PROJECT_SOURCE_DIR}/engine/platforms/pc/src)
target_link_libraries(nnh_conformance_pc PRIVATE SDL3::SDL3)
# PC only: the console remote's loopback transport and screen read-back (spec 013); the test plays nnh with
# nnh_link's sockets.
target_sources(nnh_conformance_pc PRIVATE platform_conformance/test_remote_pc.cpp)
target_link_libraries(nnh_conformance_pc PRIVATE nnh_link)
# PC only (spec 029): coverage textures draw like white RGBA texels of that alpha, read back.
target_sources(nnh_conformance_pc PRIVATE platform_conformance/test_texture_alpha_pc.cpp
                                          platform_conformance/test_language_pc.cpp
                                          platform_conformance/test_reload_shared_open_pc.cpp)
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

    # Tween setters, callbacks and finish events keep the inline size rules (spec 015 FR-5, FR-11): each file
    # compiles with -DNNH_EXPECT_OK and must fail with the size message without it.
    set(_tween_check ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only -I${PROJECT_SOURCE_DIR}/engine/core/include)
    foreach(case "setter_capture|capture too large [(]max 16 bytes[)]"
                 "big_event|event published on finish too large [(]max 32 bytes[)]")
        string(REPLACE "|" ";" case "${case}")
        list(GET case 0 name)
        list(GET case 1 expected)
        set(_file ${CMAKE_CURRENT_SOURCE_DIR}/tween/compile_fail_${name}.cpp)
        add_test(NAME tween_${name}_ok COMMAND ${_tween_check} -DNNH_EXPECT_OK ${_file})
        add_test(NAME tween_${name}_rejected COMMAND ${_tween_check} ${_file})
        set_tests_properties(tween_${name}_rejected PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
    endforeach()
    # Sprite notice callbacks keep the same inline size rule (spec 016 FR-8).
    set(_file ${CMAKE_CURRENT_SOURCE_DIR}/animation/compile_fail_callback_capture.cpp)
    add_test(NAME animation_callback_capture_ok COMMAND ${_tween_check} -DNNH_EXPECT_OK ${_file})
    add_test(NAME animation_callback_capture_rejected COMMAND ${_tween_check} ${_file})
    set_tests_properties(animation_callback_capture_rejected PROPERTIES
                         PASS_REGULAR_EXPRESSION "capture too large [(]max 16 bytes[)]")

    # AppConfig::title defaults to the game's TITLE (spec 009 FR-1); the last case proves the check bites.
    set(_title_check ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only -I${PROJECT_SOURCE_DIR}/engine/core/include)
    add_test(NAME game_title_default COMMAND ${_title_check} ${CMAKE_CURRENT_SOURCE_DIR}/app/compile_game_title.cpp)
    add_test(NAME game_title_from_build COMMAND ${_title_check} "-DNNH_GAME_TITLE=\"My Game\""
             "-DEXPECTED_TITLE=\"My Game\"" ${CMAKE_CURRENT_SOURCE_DIR}/app/compile_game_title.cpp)
    add_test(NAME game_title_mismatch_fails COMMAND ${_title_check} "-DNNH_GAME_TITLE=\"My Game\""
             ${CMAKE_CURRENT_SOURCE_DIR}/app/compile_game_title.cpp)
    set_tests_properties(game_title_mismatch_fails PROPERTIES WILL_FAIL TRUE)

    # Data schemas (spec 014 FR-1): a correct one compiles; each mistake fails to compile AND says what is wrong.
    set(_schema_check ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only -Wall -Wextra
                      -I${PROJECT_SOURCE_DIR}/engine/core/include)
    set(_schemas ${CMAKE_CURRENT_SOURCE_DIR}/data_assets/compile_fail)
    add_test(NAME data_schema_accepts_good COMMAND ${_schema_check} -Werror ${_schemas}/good.cpp)
    # Names: GCC quotes them with ASCII quotes or, in a UTF-8 locale (Linux), with 3-byte ‘ ’, so no quote is matched.
    # `~` separates alternatives (any one passes): GCC's wording, then clang's.
    foreach(case "unknown_member|damgae[^ ]* is not a member~no member named .damgae."
                 "pointer|data fields can't be pointers or references"
                 "double|double isn't a data field kind: use float"
                 "int64|64-bit integers aren't data field kinds"
                 "std_string|data fields must be trivially copyable"
                 "min_on_string|data_error_min_or_max_on_a_field_that_is_not_a_number"
                 "min_above_max|data_error_min_is_greater_than_max"
                 "listed_twice|data_error_field_listed_twice"
                 "not_aggregate|a data type must be a plain struct"
                 "too_big|a data type can be at most 16 KiB"
                 "too_many_fields|a data type can list at most 64 fields"
                 "unknown_enum_name|Fier[^ ]* was not declared~undeclared identifier .Fier.")
        string(REPLACE "|" ";" case "${case}")
        list(GET case 0 name)
        list(GET case 1 expected)
        string(REPLACE "~" ";" expected "${expected}")
        add_test(NAME data_schema_rejects_${name} COMMAND ${_schema_check} ${_schemas}/${name}.cpp)
        set_tests_properties(data_schema_rejects_${name} PROPERTIES WILL_FAIL TRUE)
        add_test(NAME data_schema_explains_${name} COMMAND ${_schema_check} ${_schemas}/${name}.cpp)
        set_tests_properties(data_schema_explains_${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
    endforeach()
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
add_test(NAME asset_index_names_maps_by_their_output
    COMMAND ${CMAKE_COMMAND} -DASSETS_SCRIPT=${PROJECT_SOURCE_DIR}/cmake/nnh_assets.cmake
            -P ${_lists}/check_output_names.cmake)
# The data index's name (spec 014 T7.4): reserved, unless the build generated it (games with a data schema).
add_test(NAME asset_index_accepts_generated_data
    COMMAND ${CMAKE_COMMAND} -DLIST=${_lists}/reserved_data.list -DGENERATED=.nnh_data
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/asset_index_generated_data.txt -P ${_index_script})
foreach(case "dotdot|no '.' or '..' folders"
             "chars|use only letters"
             "reserved|reserved"
             "reserved_data|the name .nnh_data is reserved"
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

# Tiled map converter (spec 012 T5.7, AC-3): nnh-assetc on the fixtures of data/maps (written by hand; the PNGs by
# make_fixtures.py). Each bad map must fail AND name the file and the problem.
set(_maps ${CMAKE_CURRENT_SOURCE_DIR}/data/maps)
set(_maps_out ${CMAKE_CURRENT_BINARY_DIR}/maps)
file(MAKE_DIRECTORY ${_maps_out})
# Over 4096 objects: generated here rather than committed (4097 copies of one object; ids don't matter, the limit
# is checked first).
string(REPEAT "{\"id\": 1, \"x\": 0, \"y\": 0}, " 4096 _objects)
file(WRITE ${_maps_out}/bad_too_many_objects.tmj
     "{\"orientation\": \"orthogonal\", \"width\": 2, \"height\": 1, \"tilewidth\": 16, \"tileheight\": 16,
       \"layers\": [{\"type\": \"objectgroup\", \"name\": \"things\", \"objects\": [${_objects}{\"id\": 1, \"x\": 0, \"y\": 0}]}]}\n")
add_test(NAME map_convert_accepts_good
    COMMAND nnh_assetc map ${_maps}/good.tmj ${_maps_out}/good_check.nnhmap --assets ${_maps})
add_test(NAME map_convert_accepts_good_prefabs
    COMMAND nnh_assetc map ${_maps}/good_prefabs.tmj ${_maps_out}/good_prefabs_check.nnhmap --assets ${_maps})
add_test(NAME map_convert_accepts_good_prefab_trees
    COMMAND nnh_assetc map ${_maps}/good_prefab_trees.tmj ${_maps_out}/good_prefab_trees_check.nnhmap --assets ${_maps})
add_test(NAME map_convert_accepts_good_collision_layers
    COMMAND nnh_assetc map ${_maps}/good_collision_layers.tmj ${_maps_out}/good_collision_layers_check.nnhmap
            --assets ${_maps})
foreach(case "infinite|bad_infinite.tmj: infinite maps aren't supported"
             "isometric|bad_isometric.tmj: isometric maps aren't supported"
             "hexagonal|bad_hexagonal.tmj: hexagonal maps aren't supported"
             "image_layer|bad_image_layer.tmj: layer .sky.: image layers aren't supported"
             "group_layer|bad_group_layer.tmj: layer .level.: group layers aren't supported"
             "wang_set|bad_wang_set.tmj: wang sets [(]terrains[)] aren't supported"
             "text_object|bad_text_object.tmj: object 1 in layer .things.: text objects aren't supported"
             "polygon|bad_polygon.tmj: object 1 in layer .things.: polygon objects aren't supported"
             "class_property|bad_class_property.tmj: map: property .stats.: class properties aren't supported"
             "missing_image|bad_missing_image.tmj: tileset image tiles/missing.png not found"
             "image_outside|bad_image_outside.tmj: tileset image ../images/ok_16x8.png is outside the assets folder"
             "not_power_of_two|bad_not_power_of_two.tmj: tileset image tiles/npot.png is 100x60: tileset images need power-of-two sides"
             "tile_size_4|bad_tile_size_4.tmj: tiles are 4x4: NNHGE tiles are 8 to 64 pixels per side"
             "tile_size_128|bad_tile_size_128.tmj: tiles are 128x128: NNHGE tiles are 8 to 64 pixels per side"
             "too_many_layers|bad_too_many_layers.tmj: more than 16 tile layers"
             "too_many_tilesets|bad_too_many_tilesets.tmj: more than 8 tilesets"
             "too_many_tiles|bad_too_many_tiles.tmj: the tilesets have 16384 tiles in all: a map can use at most 8191"
             "too_many_objects|bad_too_many_objects.tmj: the map has more than 4096 objects"
             "diagonal_non_square|bad_diagonal_non_square.tmj: layer .ground.: cell [(]1, 0[)] is flipped diagonally, which needs square tiles"
             "animation|bad_animation.tmj: tile 1: animation frame 1: .tileid. must be a whole number from 0 to 7"
             "compressed|bad_compressed.tmj: layer .ground.: compressed layer data [(]zlib[)] isn't supported: save the layer format as CSV"
             "broken_json|bad_broken_json.tmj: line 3, column 19: expected ',' or '}'"
             "collision_layers_type|bad_collision_layers_type.tmj: layer .water.: property .collision_layers. must be an integer"
             "collision_layers_zero|bad_collision_layers_zero.tmj: layer .water.: property .collision_layers. must not be 0: untick collision instead")
    string(REPLACE "|" ";" case "${case}")
    list(GET case 0 name)
    list(GET case 1 expected)
    set(_in ${_maps}/bad_${name}.tmj)
    if(name STREQUAL "too_many_objects")
        set(_in ${_maps_out}/bad_${name}.tmj)
    endif()
    set(_convert nnh_assetc map ${_in} ${_maps_out}/bad_${name}.nnhmap --assets ${_maps})
    add_test(NAME map_convert_rejects_${name} COMMAND ${_convert})
    set_tests_properties(map_convert_rejects_${name} PROPERTIES WILL_FAIL TRUE)
    add_test(NAME map_convert_explains_${name} COMMAND ${_convert})
    set_tests_properties(map_convert_explains_${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
endforeach()

# Data converter and index step (spec 014 T6.3, AC-3): nnh-assetc on the fixtures of data/data_assets, against the
# committed description of the tests' schema (data/data_assets/schema.nnhschema; ctest data_fixtures_schema_is_current
# keeps it equal to the compiled one). Each bad fixture starts with `# expect: <message>` lines that the converter
# must print exactly (check_convert.cmake); each index case's expected output is index/<case>.expect.
set(_dfx ${CMAKE_CURRENT_SOURCE_DIR}/data/data_assets)
set(_dfx_out ${CMAKE_CURRENT_BINARY_DIR}/data_assets_fixtures)
set(_dfx_args -DASSETC=$<TARGET_FILE:nnh_assetc> -DSCHEMA=${_dfx}/schema.nnhschema)
# Over the limits: generated here rather than committed.
string(REPEAT "x" 1000 _x1000)
string(REPEAT "# ${_x1000}\n" 66 _filler)
string(REPEAT "x" 1023 _x1023)
file(WRITE ${_dfx_out}/bad/file_too_big.enemy
     "# expect: bad/file_too_big.enemy:1: the file is larger than 64 KiB\nname = \"Goblin\"\n${_filler}")
file(WRITE ${_dfx_out}/bad/line_too_long.enemy
     "# expect: bad/line_too_long.enemy:3: the line is 1025 bytes: at most 1024\nname = \"Goblin\"\n# ${_x1023}\n")
foreach(name array_count bad_color bad_escape bare_word binary date dotted_key float_out_of_range hex inf
             inline_table int_too_big_for_type key_twice list_too_long literal_string missing_required
             missing_required_nested multiline_string nan octal out_of_range quoted_key string_too_long table_twice
             too_deep too_many_errors unknown_enum unknown_field unknown_section wrong_kind
             file_too_big line_too_long)
    set(_in ${_dfx}/bad/${name}.enemy)
    if(name MATCHES "^(file_too_big|line_too_long)$")
        set(_in ${_dfx_out}/bad/${name}.enemy)
    endif()
    add_test(NAME data_convert_rejects_${name}
        COMMAND ${CMAKE_COMMAND} ${_dfx_args} -DINPUT=${_in} -DSHOW=bad/${name}.enemy -DWORK=${_dfx_out}/work/${name}
                -P ${_dfx}/check_convert.cmake)
endforeach()
add_test(NAME data_convert_accepts_good
    COMMAND ${CMAKE_COMMAND} ${_dfx_args} -DCASE=${_dfx}/game/assets -DSHOW=assets -DWORK=${_dfx_out}/work/good
            -P ${_dfx}/check_index.cmake)
foreach(name duplicate_name missing_ref missing_texture sound_to_image texture_to_sound wrong_type_ref)
    add_test(NAME data_index_rejects_${name}
        COMMAND ${CMAKE_COMMAND} ${_dfx_args} -DCASE=${_dfx}/index/${name} -DSHOW=${name}
                -DEXPECT=${_dfx}/index/${name}.expect -DWORK=${_dfx_out}/work/index_${name}
                -P ${_dfx}/check_index.cmake)
endforeach()

# The good fixture converted at build time, for the code-vs-file test (spec 012 T5.9): nnh_tests reads it from here.
add_custom_command(OUTPUT ${_maps_out}/good.nnhmap
    COMMAND nnh_assetc map ${_maps}/good.tmj ${_maps_out}/good.nnhmap --assets ${_maps}
            --depfile ${_maps_out}/good.nnhmap.d
    DEPENDS ${_maps}/good.tmj nnh_assetc
    DEPFILE ${_maps_out}/good.nnhmap.d
    COMMENT "Map tests/data/maps/good.tmj"
    VERBATIM)
# The platformer's level 1, for the collision_grid() regression test (spec 019 T0.2).
set(_platformer_assets ${PROJECT_SOURCE_DIR}/examples/platformer/assets)
add_custom_command(OUTPUT ${_maps_out}/level1.nnhmap
    COMMAND nnh_assetc map ${_platformer_assets}/levels/level1.tmj ${_maps_out}/level1.nnhmap
            --assets ${_platformer_assets} --depfile ${_maps_out}/level1.nnhmap.d
    DEPENDS ${_platformer_assets}/levels/level1.tmj nnh_assetc
    DEPFILE ${_maps_out}/level1.nnhmap.d
    COMMENT "Map examples/platformer/assets/levels/level1.tmj (tests)"
    VERBATIM)
# The collision layers fixture (spec 019 T5.2), loaded by test_map_load_collision_layers.cpp.
add_custom_command(OUTPUT ${_maps_out}/good_collision_layers.nnhmap
    COMMAND nnh_assetc map ${_maps}/good_collision_layers.tmj ${_maps_out}/good_collision_layers.nnhmap
            --assets ${_maps} --depfile ${_maps_out}/good_collision_layers.nnhmap.d
    DEPENDS ${_maps}/good_collision_layers.tmj nnh_assetc
    DEPFILE ${_maps_out}/good_collision_layers.nnhmap.d
    COMMENT "Map tests/data/maps/good_collision_layers.tmj"
    VERBATIM)
# The prefab objects fixture (spec 021 T5.1), loaded by tests/prefabs/test_map_objects.cpp.
add_custom_command(OUTPUT ${_maps_out}/good_prefabs.nnhmap
    COMMAND nnh_assetc map ${_maps}/good_prefabs.tmj ${_maps_out}/good_prefabs.nnhmap
            --assets ${_maps} --depfile ${_maps_out}/good_prefabs.nnhmap.d
    DEPENDS ${_maps}/good_prefabs.tmj nnh_assetc
    DEPFILE ${_maps_out}/good_prefabs.nnhmap.d
    COMMENT "Map tests/data/maps/good_prefabs.tmj"
    VERBATIM)
add_custom_command(OUTPUT ${_maps_out}/good_prefab_trees.nnhmap
    COMMAND nnh_assetc map ${_maps}/good_prefab_trees.tmj ${_maps_out}/good_prefab_trees.nnhmap
            --assets ${_maps} --depfile ${_maps_out}/good_prefab_trees.nnhmap.d
    DEPENDS ${_maps}/good_prefab_trees.tmj nnh_assetc
    DEPFILE ${_maps_out}/good_prefab_trees.nnhmap.d
    COMMENT "Map tests/data/maps/good_prefab_trees.tmj"
    VERBATIM)
add_custom_target(nnh_test_maps DEPENDS ${_maps_out}/good.nnhmap ${_maps_out}/level1.nnhmap
                                        ${_maps_out}/good_collision_layers.nnhmap ${_maps_out}/good_prefabs.nnhmap
                                        ${_maps_out}/good_prefab_trees.nnhmap)
add_dependencies(nnh_tests nnh_test_maps)
target_compile_definitions(nnh_tests PRIVATE NNH_TEST_MAPS_DIR="${_maps_out}")

# Aseprite converter (spec 016 T7.5, AC-4): nnh-assetc sprite on the fixtures of data/aseprite (make_fixtures.py).
# Each bad export must fail AND say `file:line: reason`.
set(_ase ${CMAKE_CURRENT_SOURCE_DIR}/data/aseprite)
set(_ase_out ${CMAKE_CURRENT_BINARY_DIR}/aseprite)
file(MAKE_DIRECTORY ${_ase_out})
foreach(good good good_array)
    add_test(NAME sprite_convert_accepts_${good}
        COMMAND nnh_assetc sprite ${_ase}/${good}.ase.json ${_ase_out}/${good}_check.nnhsprite --assets ${_ase})
endforeach()
foreach(case "rotated|bad_rotated.ase.json:24: frame 1 .* rotated frames aren't supported"
             "direction|bad_direction.ase.json:99: frame tag .run.: unknown direction .sideways."
             "range|bad_range.ase.json:99: frame tag .run.: .to. must be a whole number from 0 to 3"
             "duplicate|bad_duplicate.ase.json:106: frame tag .run.: two clips have this name"
             "duration|bad_duration.ase.json:3: frame 0 .*: .duration. must be a whole number from 1 to 65535"
             "image|bad_image.ase.json:91: .meta.: .image. is .hero.png.: it must be bad_image.png"
             "size|bad_size.ase.json:93: .meta.: .size. is 128x16 but bad_size.png is 64x16"
             "marker|bad_marker.ase.json:99: marker tag .@hit=lots.: the value after '=' must be a whole number"
             "repeat|bad_repeat.ase.json:99: frame tag .run.: .repeat. must be a whole number"
             "missing_png|bad_missing_png.ase.json: bad_missing_png.png not found"
             "broken_json|bad_broken_json.ase.json: line 3, column 20: expected a key")
    string(REPLACE "|" ";" case "${case}")
    list(GET case 0 name)
    list(GET case 1 expected)
    set(_convert nnh_assetc sprite ${_ase}/bad_${name}.ase.json ${_ase_out}/bad_${name}.nnhsprite --assets ${_ase})
    add_test(NAME sprite_convert_rejects_${name} COMMAND ${_convert})
    set_tests_properties(sprite_convert_rejects_${name} PROPERTIES WILL_FAIL TRUE)
    add_test(NAME sprite_convert_explains_${name} COMMAND ${_convert})
    set_tests_properties(sprite_convert_explains_${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
endforeach()
# The good export converted at build time, read back by nnh_tests (tests/animation/test_sprite_aseprite.cpp).
add_custom_command(OUTPUT ${_ase_out}/good.nnhsprite
    COMMAND nnh_assetc sprite ${_ase}/good.ase.json ${_ase_out}/good.nnhsprite --assets ${_ase}
            --depfile ${_ase_out}/good.nnhsprite.d
    DEPENDS ${_ase}/good.ase.json nnh_assetc
    DEPFILE ${_ase_out}/good.nnhsprite.d
    COMMENT "Sprite tests/data/aseprite/good.ase.json"
    VERBATIM)
add_custom_target(nnh_test_sprites DEPENDS ${_ase_out}/good.nnhsprite)
add_dependencies(nnh_tests nnh_test_sprites)
target_compile_definitions(nnh_tests PRIVATE NNH_TEST_SPRITES_DIR="${_ase_out}")

# 3D foundation (spec 026): math, meshes, the 3D pass, the CPU rasteriser, the example's self-check.
target_sources(nnh_tests PRIVATE
    gfx3d/test_eyes.cpp
    gfx3d/test_pixels_fake.cpp
    gfx3d/test_probe.cpp
    gfx3d/test_record.cpp
    gfx3d/test_screen_pass.cpp
    gfx3d/test_screen_pass_stereo.cpp
    gfx3d/test_screen_project.cpp
    math3d/test_conventions.cpp
    math3d/test_mat4.cpp
    math3d/test_math3d_accuracy.cpp
    math3d/test_math3d_exact.cpp
    math3d/test_projection.cpp
    math3d/test_quat.cpp
    math3d/test_quat_interp.cpp
    math3d/test_vec3.cpp
    mesh/test_create_mesh.cpp
    mesh/test_load_mesh.cpp
    mesh/test_mesh_build.cpp
    mesh/test_mesh_validate.cpp
    mesh/test_primitives.cpp
    models_example/test_self_check.cpp
    raster3d/test_raster.cpp
    ${PROJECT_SOURCE_DIR}/examples/models/self_check.cpp
    ${PROJECT_SOURCE_DIR}/examples/models/self_check_stereo.cpp
)
target_link_libraries(nnh_tests PRIVATE nnh::raster3d)
set_source_files_properties(${PROJECT_SOURCE_DIR}/examples/models/self_check.cpp
                            ${PROJECT_SOURCE_DIR}/examples/models/self_check_stereo.cpp PROPERTIES
                            COMPILE_OPTIONS -ffp-contract=off)

# libFuzzer executables (clang only): cmake --preset pc-debug -DNNH_BUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
# then run e.g. build/pc-debug/tests/fuzz_save -max_total_time=60
option(NNH_BUILD_FUZZERS "Build libFuzzer targets (requires clang)" OFF)
if(NNH_BUILD_FUZZERS)
    # The code under test lives in these libraries: instrument it for coverage and ASan, or libFuzzer
    # mutates blind (it reported "cov: 1" before this was added).
    foreach(lib nnh_core nnh_stb_vorbis nnh_fuzz_targets)
        target_compile_options(${lib} PRIVATE -fsanitize=fuzzer-no-link,address)
    endforeach()
    foreach(name path save asset_index wav ogg remote tilemap data data_index sprite bindings world_ops prefab_ops
                 hierarchy_ops transform)
        add_executable(fuzz_${name} fuzz/libfuzzer_${name}.cpp)
        target_link_libraries(fuzz_${name} PRIVATE nnh_fuzz_targets)
        target_compile_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address)
    endforeach()
endif()

# 3D foundation (spec 026): fuzz targets of the two new parsers (the .nnhmesh reader, the glTF reader in nnh-assetc).
target_sources(nnh_fuzz_targets PRIVATE fuzz/fuzz_mesh.cpp)
target_sources(nnh_tests PRIVATE fuzz/test_fuzz_mesh.cpp)
if(NNH_BUILD_FUZZERS)
    add_executable(fuzz_mesh_reader fuzz/libfuzzer_mesh_reader.cpp)
    target_link_libraries(fuzz_mesh_reader PRIVATE nnh_fuzz_targets)
    target_compile_options(fuzz_mesh_reader PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_mesh_reader PRIVATE -fsanitize=fuzzer,address)
endif()
# The 3D contract's conformance (spec 026): the fake platform and the PC simulator (headless).
target_sources(nnh_conformance_fake PRIVATE platform_conformance/test_video3d.cpp)
target_sources(nnh_conformance_pc PRIVATE platform_conformance/test_video3d.cpp)
# The models example (spec 026 AC-7): its check mode in the simulator (math golden, probe pixels) and its console script
# played with --pc.
if(TARGET models AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME models_self_check_pc COMMAND models)
    set_tests_properties(models_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;MODELS_DEMO=check;NNH_EXIT_AFTER_FRAMES=80"
        PASS_REGULAR_EXPRESSION "3d pixels: PASS" FAIL_REGULAR_EXPRESSION "3d: FAIL|3d pixels: FAIL|\] E " TIMEOUT 120)
    get_filename_component(_preset ${CMAKE_BINARY_DIR} NAME)
    add_test(NAME models_check_script_pc
             COMMAND $<TARGET_FILE:nnh> run models --pc --no-build --script examples/models/tests/check.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(models_check_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 300
        RESOURCE_LOCK nnh_remote_port)
    # 026b: the stereo check mode with the simulator's slider up, and the stereo console script (slider command, both
    # eyes' screenshots, the stereo golden and probes).
    add_test(NAME models_stereo_check_pc COMMAND models)
    set_tests_properties(models_stereo_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;MODELS_DEMO=check;NNH_SIM_SLIDER=1;NNH_EXIT_AFTER_FRAMES=150"
        PASS_REGULAR_EXPRESSION "3d pixels stereo: PASS" FAIL_REGULAR_EXPRESSION "3d: FAIL|3d-stereo: FAIL|3d pixels: FAIL|\] E "
        TIMEOUT 120)
    add_test(NAME models_stereo_script_pc
             COMMAND $<TARGET_FILE:nnh> run models --pc --no-build --script examples/models/tests/stereo.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(models_stereo_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 300
        RESOURCE_LOCK nnh_remote_port)
endif()
# Live values (spec 030): every test of the spec in one block.
target_sources(nnh_tests PRIVATE
    live/test_live_format.cpp
    live/test_live_names.cpp
    live/test_live_steps.cpp
    app/test_dev_pause.cpp
    app/test_dev_pause_state.cpp
    audio/test_audio_hold.cpp
    debug/test_dev_controls.cpp
    debug/test_overlay_pages.cpp
    gfx/test_gfx_replay.cpp
    gfx/test_gfx_replay_tiles.cpp
    live/test_live_apply.cpp
    live/test_live_cost.cpp
    live/test_live_dump.cpp
    live/test_live_panel_draw.cpp
    live/test_live_panel_model.cpp
    live/test_live_panel_touch.cpp
    live/test_live_random_ops.cpp
    live/test_live_registry.cpp
    live/test_live_release.cpp
    live/test_live_tune.cpp
    live/test_live_watch.cpp
    fuzz/test_fuzz_live_wire.cpp
    remote/test_live_wire.cpp
    remote/test_remote_live.cpp
    fuzz/test_fuzz_live_remote.cpp)
# The live values' remote fuzz target (T5.3, AC-5): mini-fuzz above, libFuzzer on the build VM.
target_sources(nnh_fuzz_targets PRIVATE fuzz/fuzz_live_remote.cpp)
if(NNH_BUILD_FUZZERS)
    add_executable(fuzz_live_remote fuzz/libfuzzer_live_remote.cpp)
    target_link_libraries(fuzz_live_remote PRIVATE nnh_fuzz_targets)
    target_compile_options(fuzz_live_remote PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_live_remote PRIVATE -fsanitize=fuzzer,address)
endif()
# Every float's text read back (T1.4): minutes long, so label "slow" and off unless NNH_SLOW_TESTS (the build VM, T9.1).
option(NNH_SLOW_TESTS "Run the slow exhaustive tests (spec 030's all-floats text check)" OFF)
add_executable(live_float_text_exhaustive live/exhaustive_float_text.cpp)
target_link_libraries(live_float_text_exhaustive PRIVATE nnh::core)
target_include_directories(live_float_text_exhaustive PRIVATE ${PROJECT_SOURCE_DIR}/engine/core/src)
nnh_engine_options(live_float_text_exhaustive)
add_test(NAME live_float_text_exhaustive COMMAND live_float_text_exhaustive)
set_tests_properties(live_float_text_exhaustive PROPERTIES LABELS slow TIMEOUT 3600
                     PASS_REGULAR_EXPRESSION "live float text exhaustive: PASS")
if(NOT NNH_SLOW_TESTS)
    set_tests_properties(live_float_text_exhaustive PROPERTIES DISABLED TRUE)
endif()
# The value text and stepping tests compare bits with expressions folded by the compiler: no fused multiply-add.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(live/test_live_steps.cpp PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
# Names checked at compile time (T1.1, FR-1): a good literal compiles with -Werror; each broken rule fails to compile
# AND names the rule.
if(NOT MSVC)
    set(_live_check ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only -Wall -Wextra -I${PROJECT_SOURCE_DIR}/engine/core/include)
    set(_live_cases ${CMAKE_CURRENT_SOURCE_DIR}/live/compile_fail)
    add_test(NAME live_name_good_compiles COMMAND ${_live_check} -Werror ${_live_cases}/good.cpp)
    foreach(_case too_long bad_character empty_part too_deep)
        add_test(NAME live_name_${_case}_fails COMMAND ${_live_check} ${_live_cases}/${_case}.cpp)
        set_tests_properties(live_name_${_case}_fails PROPERTIES WILL_FAIL TRUE)
        add_test(NAME live_name_${_case}_explains COMMAND ${_live_check} ${_live_cases}/${_case}.cpp)
        set_tests_properties(live_name_${_case}_explains PROPERTIES PASS_REGULAR_EXPRESSION "live_name_${_case}")
    endforeach()
endif()
# The live example's self-check (spec 030 T7.1, AC-2): its goldens on every host compiler.
target_sources(nnh_tests PRIVATE live_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/live/self_check.cpp
                                 ${PROJECT_SOURCE_DIR}/examples/live/sim.cpp)
target_include_directories(nnh_tests PRIVATE ${PROJECT_SOURCE_DIR}/examples)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${PROJECT_SOURCE_DIR}/examples/live/self_check.cpp ${PROJECT_SOURCE_DIR}/examples/live/sim.cpp
                                PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
# The live example in the simulator (T7.2, T7.5): its self-check, and its console script played with --pc.
if(TARGET live AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME live_self_check_pc COMMAND live)
    set_tests_properties(live_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;LIVE_DEMO=check;NNH_EXIT_AFTER_FRAMES=30"
        PASS_REGULAR_EXPRESSION "live: PASS defaults" FAIL_REGULAR_EXPRESSION "\] E " TIMEOUT 120)
    get_filename_component(_live_preset ${CMAKE_BINARY_DIR} NAME)
    add_test(NAME live_check_script_pc
             COMMAND $<TARGET_FILE:nnh> run live --pc --no-build --script examples/live/tests/check.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(live_check_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_live_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 300
        RESOURCE_LOCK nnh_remote_port)
    add_test(NAME platformer_paused_equals_running_pc
             COMMAND ${CMAKE_COMMAND} -DNNH=$<TARGET_FILE:nnh> -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/live_example/paused_shot.cmake)
    set_tests_properties(platformer_paused_equals_running_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_live_preset}"
        PASS_REGULAR_EXPRESSION "paused shot: PASS" TIMEOUT 300 RESOURCE_LOCK nnh_remote_port)
    add_test(NAME live_keep_pc COMMAND ${CMAKE_COMMAND} -DNNH=$<TARGET_FILE:nnh> -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                                       -P ${CMAKE_CURRENT_SOURCE_DIR}/live_example/keep.cmake)
    set_tests_properties(live_keep_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_live_preset}"
        PASS_REGULAR_EXPRESSION "live keep: PASS" TIMEOUT 300 RESOURCE_LOCK nnh_remote_port)
    add_test(NAME live_panel_script_pc
             COMMAND $<TARGET_FILE:nnh> run live --pc --no-build --script examples/live/tests/panel.nnhs
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(live_panel_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_live_preset}"
        PASS_REGULAR_EXPRESSION "nnh: PASS" FAIL_REGULAR_EXPRESSION "nnh: FAIL" TIMEOUT 300
        RESOURCE_LOCK nnh_remote_port)
endif()

# Hot reload of assets (spec 031): every test of the spec in one block.
target_sources(nnh_tests PRIVATE
    assets/test_assets_overlay.cpp
    data_assets/test_data_reload.cpp
    gfx/test_texture_reload.cpp
    audio/test_sound_reload.cpp
    font/test_font_reload.cpp
    mesh/test_mesh_reload.cpp
    loc/test_loc_reload.cpp
    ui/test_ui_reload.cpp
    reload/test_reload_apply.cpp
    reload/test_reload_batch.cpp
    reload/test_reload_cost.cpp
    reload/test_reload_files.cpp
    reload/test_reload_names.cpp
    reload/test_reload_overlay.cpp
    reload/test_reload_queries.cpp
    reload/test_reload_random.cpp
    reload/test_reload_release.cpp
    reload/test_reload_receiver.cpp
    reload/test_reload_wire.cpp
    remote/test_remote_reader_payload.cpp
    tilemap/test_map_reload.cpp
    fuzz/test_fuzz_reload.cpp)
# The reload receiver's fuzz target (T1.6, AC-6): mini-fuzz above, libFuzzer on the build VM.
target_sources(nnh_fuzz_targets PRIVATE fuzz/fuzz_reload_receiver.cpp)
if(NNH_BUILD_FUZZERS)
    add_executable(fuzz_reload_receiver fuzz/libfuzzer_reload_receiver.cpp)
    target_link_libraries(fuzz_reload_receiver PRIVATE nnh_fuzz_targets)
    target_compile_options(fuzz_reload_receiver PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_reload_receiver PRIVATE -fsanitize=fuzzer,address)
endif()
# The hot reload smoke script (T3.7): a data asset swapped and restored in the simulator, the source tree unchanged.
if(TARGET data AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    get_filename_component(_reload_preset ${CMAKE_BINARY_DIR} NAME)
    add_test(NAME reload_data_smoke_pc
             COMMAND ${CMAKE_COMMAND} -DNNH=$<TARGET_FILE:nnh> -DGAME=data -DSCRIPT=examples/data/tests/reload.nnhs
                     -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/reload_example/script_clean.cmake)
    set_tests_properties(reload_data_smoke_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_reload_preset}"
        PASS_REGULAR_EXPRESSION "reload script: PASS" FAIL_REGULAR_EXPRESSION "reload script: FAIL" TIMEOUT 300
        RESOURCE_LOCK nnh_remote_port)
endif()
# The hot reload example (spec 031 AC-1�AC-3): every implemented kind swapped and applied with the PC's self-check sums,
# then the refusals (a broken map, a changed schema); the example's folder unchanged afterwards.
if(TARGET reload AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    get_filename_component(_reload_preset ${CMAKE_BINARY_DIR} NAME)
    foreach(_script check errors)
        add_test(NAME reload_${_script}_script_pc
                 COMMAND ${CMAKE_COMMAND} -DNNH=$<TARGET_FILE:nnh> -DGAME=reload -DSCRIPT=examples/reload/tests/${_script}.nnhs
                         -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/reload_example/script_clean.cmake)
        set_tests_properties(reload_${_script}_script_pc PROPERTIES
            ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_reload_preset}"
            PASS_REGULAR_EXPRESSION "reload script: PASS" FAIL_REGULAR_EXPRESSION "reload script: FAIL" TIMEOUT 600
            RESOURCE_LOCK nnh_remote_port)
    endforeach()
endif()
# AC-4b: the sprites example (no reload-aware code) picks up a swapped PNG; its folder unchanged afterwards.
if(TARGET sprites AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    get_filename_component(_reload_preset ${CMAKE_BINARY_DIR} NAME)
    add_test(NAME sprites_reload_script_pc
             COMMAND ${CMAKE_COMMAND} -DNNH=$<TARGET_FILE:nnh> -DGAME=sprites -DSCRIPT=examples/sprites/tests/reload.nnhs
                     -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/reload_example/script_clean.cmake)
    set_tests_properties(sprites_reload_script_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;NNH_PRESET=${_reload_preset}"
        PASS_REGULAR_EXPRESSION "reload script: PASS" FAIL_REGULAR_EXPRESSION "reload script: FAIL" TIMEOUT 300
        RESOURCE_LOCK nnh_remote_port)
endif()
