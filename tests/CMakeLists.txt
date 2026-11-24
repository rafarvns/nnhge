# Tests (docs/process/tdd.md). Layout mirrors engine/core/src: tests/<module>/test_<concept>.cpp.
# List new files here (explicit lists, no file(GLOB)).

add_library(nnh_test_framework STATIC framework/test_main.cpp)
target_include_directories(nnh_test_framework PUBLIC framework)
target_compile_features(nnh_test_framework PUBLIC cxx_std_20)

# Fuzz target bodies, shared by the unit tests (mini-fuzz) and the libFuzzer executables below.
add_library(nnh_fuzz_targets STATIC fuzz/fuzz_bindings.cpp fuzz/fuzz_data.cpp fuzz/fuzz_data_index.cpp fuzz/fuzz_sprite.cpp
                                     fuzz/fuzz_targets.cpp fuzz/fuzz_tilemap.cpp fuzz/fuzz_world_ops.cpp
                                     entities/model_driver.cpp fuzz/fuzz_prefab_ops.cpp
                                     ${PROJECT_SOURCE_DIR}/examples/prefabs/self_check.cpp
                                     ${PROJECT_SOURCE_DIR}/examples/prefabs/self_check_defs.cpp
                                     ${PROJECT_SOURCE_DIR}/examples/prefabs/self_check_sides.cpp)
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
    fuzz/test_fuzz_world_ops.cpp
    gfx/test_camera_zoom_clamp.cpp
    gfx/test_draw_calls.cpp
    gfx/test_draw_list.cpp
    gfx/test_draw_list_zoom.cpp
    gfx/test_texture_rules.cpp
    gfx/test_texture_table.cpp
    gfx/test_tile_batches.cpp
    gfx/test_tile_submit.cpp
    gfx/test_textures.cpp
    gfx/test_tile_chunks.cpp
    gfx/test_images.cpp
    gfx/test_zoom1_regression.cpp
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
    prefabs/test_variants.cpp
    remote/test_remote_input.cpp
    remote/test_remote_reader.cpp
    remote/test_remote_records.cpp
    remote/test_remote_schedule.cpp
    remote/test_remote_wire.cpp
    result/test_result.cpp
    scene/test_run.cpp
    scene/test_scene_attachments.cpp
    scene/test_scene_stack.cpp
    system/test_system.cpp
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
                                     remote/test_remote_session.cpp actions/test_action_input_paths.cpp)
    # Shutdown hooks run before the leak report, which Release doesn't have (spec 014 T3.3).
    target_sources(nnh_tests PRIVATE app/test_shutdown_hooks.cpp)
endif()
# Sound fixtures (tests/data/sounds, made by make_sounds.py) are read from the source tree.
target_compile_definitions(nnh_tests PRIVATE NNH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_link_libraries(nnh_tests PRIVATE nnh_test_framework nnh_fuzz_targets nnh::core nnh::platform_fake)
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
target_sources(nnh_tests PRIVATE platform_3ds/test_hbldr_args.cpp ${_3ds_src}/hbldr_args.cpp)
target_include_directories(nnh_tests PRIVATE ${_3ds_src})
# The platformer's automatic gate (spec 013 FR-6) and its level logic (spec 012 stage 7: camera, coins, replay,
# 60 Hz meter, player physics) are pure files of the example, tested like core logic.
set(_platformer ${PROJECT_SOURCE_DIR}/examples/platformer)
target_sources(nnh_tests PRIVATE
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
# The queries example's self-check in the simulator (spec 019 AC-6): the verdict the console script waits for.
if(TARGET queries AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME queries_self_check_pc COMMAND queries)
    set_tests_properties(queries_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;QUERIES_DEMO=check;NNH_EXIT_AFTER_FRAMES=90"
        PASS_REGULAR_EXPRESSION "queries: PASS 4ACAA177" FAIL_REGULAR_EXPRESSION "queries: FAIL" TIMEOUT 120)
endif()
# The entities example's self-check in the simulator (spec 020 T7.4, AC-6): the verdict the console script waits for.
if(TARGET entities AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME entities_self_check_pc COMMAND entities)
    set_tests_properties(entities_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;ENTITIES_DEMO=check;NNH_EXIT_AFTER_FRAMES=70"
        PASS_REGULAR_EXPRESSION "entities: PASS C8FB9A41" FAIL_REGULAR_EXPRESSION "entities: FAIL" TIMEOUT 120)
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
# The data example's self-check in the simulator (spec 014 T8.3): the line the console script expects.
if(TARGET data AND NNH_PLATFORM STREQUAL "pc" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME data_self_check_pc COMMAND data)
    set_tests_properties(data_self_check_pc PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=software;NNH_SIM_RENDERER=sdl;SDL_AUDIO_DRIVER=dummy;DATA_DEMO=check;NNH_EXIT_AFTER_FRAMES=10"
        PASS_REGULAR_EXPRESSION "data check: PASS checksum 0xD5BD3020 [(]4 enemies, 3 items, 1 settings, index 595 bytes[)]"
        FAIL_REGULAR_EXPRESSION "data check: FAIL|\\] E " TIMEOUT 120)
endif()
target_include_directories(nnh_tests PRIVATE ${PROJECT_SOURCE_DIR}/examples)
# The tweens example's self-check (spec 015 T6.1): its golden checksum must hold on every host compiler.
target_sources(nnh_tests PRIVATE tweens/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/tweens/self_check.cpp)
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
# The queries example's self-check (spec 019 T7.3, AC-2): its golden checksum on every host compiler.
target_sources(nnh_tests PRIVATE queries_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/queries/self_check.cpp)
# The entities example's self-check (spec 020 T5.4, AC-2): its golden checksum on every host compiler.
target_sources(nnh_tests PRIVATE entities_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/entities/self_check.cpp)
# The prefabs example's self-check (spec 021 T5.4, AC-2): prefab spawns against hand-written ones, golden checksum.
set(_prefabs_example ${PROJECT_SOURCE_DIR}/examples/prefabs)
# (Its sources are compiled into nnh_fuzz_targets, which nnh_tests links: the fuzz target drives the same operations.)
target_sources(nnh_tests PRIVATE prefabs_example/test_self_check.cpp)
# The actions example's self-check and log lines (spec 017 T6.4).
target_sources(nnh_tests PRIVATE actions_example/test_self_check.cpp ${PROJECT_SOURCE_DIR}/examples/actions/action_log.cpp
                                 ${PROJECT_SOURCE_DIR}/examples/actions/self_check.cpp)
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
add_custom_target(nnh_test_maps DEPENDS ${_maps_out}/good.nnhmap ${_maps_out}/level1.nnhmap
                                        ${_maps_out}/good_collision_layers.nnhmap ${_maps_out}/good_prefabs.nnhmap)
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

# libFuzzer executables (clang only): cmake --preset pc-debug -DNNH_BUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
# then run e.g. build/pc-debug/tests/fuzz_save -max_total_time=60
option(NNH_BUILD_FUZZERS "Build libFuzzer targets (requires clang)" OFF)
if(NNH_BUILD_FUZZERS)
    # The code under test lives in these libraries: instrument it for coverage and ASan, or libFuzzer
    # mutates blind (it reported "cov: 1" before this was added).
    foreach(lib nnh_core nnh_stb_vorbis nnh_fuzz_targets)
        target_compile_options(${lib} PRIVATE -fsanitize=fuzzer-no-link,address)
    endforeach()
    foreach(name path save asset_index wav ogg remote tilemap data data_index sprite bindings world_ops prefab_ops)
        add_executable(fuzz_${name} fuzz/libfuzzer_${name}.cpp)
        target_link_libraries(fuzz_${name} PRIVATE nnh_fuzz_targets)
        target_compile_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer,address)
    endforeach()
endif()
