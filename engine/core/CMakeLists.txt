# nnh::core — the portable engine. Never depends on a system SDK.
# Sources are listed explicitly (no file(GLOB)); see docs/tech/cmake.md.

# Platform contract: what every platform library implements and nnh::core calls.
add_library(nnh_core_contract INTERFACE)
add_library(nnh::core_contract ALIAS nnh_core_contract)
target_include_directories(nnh_core_contract INTERFACE contract include)

add_library(nnh_core STATIC
    src/animation/animator.cpp
    src/animation/animator_advance.cpp
    src/animation/animator_notices.cpp
    src/animation/animator_queries.cpp
    src/animation/animator_state.cpp
    src/animation/animator_walk.cpp
    src/animation/sprite_clips.cpp
    src/animation/sprite_copy.cpp
    src/animation/sprite_frames.cpp
    src/animation/sprite_layout.cpp
    src/animation/sprite_library.cpp
    src/animation/sprite_load.cpp
    src/animation/sprite_markers.cpp
    src/animation/sprite_sequence.cpp
    src/animation/sprite_states.cpp
    src/animation/sprite_validate.cpp
    src/app/app.cpp
    src/app/data_folder.cpp
    src/collision/aabb.cpp
    src/collision/grid.cpp
    src/collision/grid_move.cpp
    src/collision/grid_query.cpp
    src/core/crc32.cpp
    src/core/crc32_update.cpp
    src/core/input_hooks.cpp
    src/core/shutdown_hooks.cpp
    src/crash/crash.cpp
    src/crash/report.cpp
    src/data/data_apply.cpp
    src/data/data_index.cpp
    src/data/data_list.cpp
    src/data/data_load.cpp
    src/data/data_validate.cpp
    src/data/enum_names.cpp
    src/data/index_validate.cpp
    src/data/schema_check.cpp
    src/data/schema_fingerprint.cpp
    src/data/schema_registry.cpp
    src/data/schema_write.cpp
    src/assets/asset_index.cpp
    src/audio/audio.cpp
    src/audio/mix.cpp
    src/audio/music.cpp
    src/audio/music_track.cpp
    src/audio/ogg_pages.cpp
    src/audio/ogg_reader.cpp
    src/audio/ogg_seek.cpp
    src/audio/sound_decode.cpp
    src/audio/sounds.cpp
    src/audio/voice_api.cpp
    src/audio/voice_table.cpp
    src/audio/wav_reader.cpp
    src/audio/wav_stream.cpp
    src/assets/asset_path.cpp
    src/assets/assets.cpp
    src/debug/debug.cpp
    src/debug/debug_list.cpp
    src/debug/debug_pass.cpp
    src/debug/overlay.cpp
    src/debug/profiler.cpp
    src/events/bus.cpp
    src/events/event_queue.cpp
    src/events/subscriber_table.cpp
    src/events/subscription.cpp
    src/fs/files.cpp
    src/fs/path.cpp
    src/fs/save.cpp
    src/fs/save_format.cpp
    src/gfx/draw2d.cpp
    src/gfx/draw_text.cpp
    src/gfx/gfx.cpp
    src/gfx/gfx_frame.cpp
    src/gfx/image_registry_default.cpp
    src/gfx/texture_table.cpp
    src/gfx/tile_batches.cpp
    src/gfx/textures.cpp
    src/input/dpad_mirror.cpp
    src/input/input.cpp
    src/input/stick.cpp
    src/log/engine_channels.cpp
    src/log/file_sink.cpp
    src/log/format.cpp
    src/log/log.cpp
    src/memory/memory.cpp
    src/result/error.cpp
    src/scene/scene_attachments.cpp
    src/scene/scene_notify.cpp
    src/scene/scene_stack.cpp
    src/scene/scenes.cpp
    src/remote/remote_input.cpp
    src/remote/remote_reader.cpp
    src/remote/remote_schedule.cpp
    src/remote/remote_screenshot.cpp
    src/remote/remote_session.cpp
    src/remote/remote_wire.cpp
    src/system/system.cpp
    src/text/builtin_font.cpp
    src/text/font_atlas.cpp
    src/text/font_texture.cpp
    src/text/layout.cpp
    src/text/utf8.cpp
    src/text/utf8_valid.cpp
    src/tilemap/map_animations.cpp
    src/tilemap/map_build.cpp
    src/tilemap/map_collision.cpp
    src/tilemap/map_collision_layers.cpp
    src/tilemap/map_copy.cpp
    src/tilemap/map_draw.cpp
    src/tilemap/map_filtered_grid.cpp
    src/tilemap/map_layout.cpp
    src/tilemap/map_load.cpp
    src/tilemap/map_load_layers.cpp
    src/tilemap/map_objects.cpp
    src/tilemap/map_properties.cpp
    src/tilemap/map_property_table.cpp
    src/tilemap/map_runtime.cpp
    src/tilemap/map_strings.cpp
    src/tilemap/map_tiles.cpp
    src/tilemap/map_validate.cpp
    src/tilemap/map_validate_content.cpp
    src/tilemap/tile_animation.cpp
    src/tilemap/tilemap.cpp
    src/time/clock.cpp
    src/time/fixed_step.cpp
)
# Tweens and timers (spec 015): every float operation of the module lives in these files. No fused multiply-add,
# so the curves and the tweened values have the same bits on every compiler and system (plan 015 "Determinism").
set(_nnh_tween_sources
    src/tween/ease.cpp
    src/tween/ease_math.cpp
    src/tween/sequence_advance.cpp
    src/tween/sequence_build.cpp
    src/tween/sequence_steps.cpp
    src/tween/timer.cpp
    src/tween/tween_advance.cpp
    src/tween/tween_commands.cpp
    src/tween/tween_finish.cpp
    src/tween/tween_log.cpp
    src/tween/tween_scene.cpp
    src/tween/tween_set.cpp
    src/tween/tween_start.cpp
    src/tween/tween_update.cpp
    src/tween/tween_values.cpp
)
target_sources(nnh_core PRIVATE ${_nnh_tween_sources})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${_nnh_tween_sources} PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
# Input actions (spec 017): every float comparison of the module lives in these files; no fused multiply-add, so the
# thresholds, dead zones and combined values are the same on every compiler and system (FR-22).
set(_nnh_actions_sources
    src/actions/action_combine.cpp
    src/actions/action_conflicts.cpp
    src/actions/action_contexts.cpp
    src/actions/action_debug.cpp
    src/actions/action_declare.cpp
    src/actions/action_edit.cpp
    src/actions/action_eval.cpp
    src/actions/action_events.cpp
    src/actions/action_hook.cpp
    src/actions/action_listen.cpp
    src/actions/action_queries.cpp
    src/actions/action_set.cpp
    src/actions/action_slots.cpp
    src/actions/bindings_apply.cpp
    src/actions/bindings_format.cpp
    src/actions/bindings_io.cpp
    src/actions/input_view.cpp
    src/actions/read_slots.cpp
    src/actions/source_eval.cpp
    src/actions/source_names.cpp
)
target_sources(nnh_core PRIVATE ${_nnh_actions_sources})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${_nnh_actions_sources} PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
# Camera helpers (spec 018): every float operation of the rig lives in these files; no fused multiply-add, so the
# followed and shaken positions have the same bits on every compiler and on the 3DS (FR-12).
set(_nnh_camera_sources
    src/camera/camera_apply.cpp
    src/camera/camera_debug.cpp
    src/camera/camera_follow.cpp
    src/camera/camera_layout.cpp
    src/camera/camera_limits.cpp
    src/camera/camera_rig.cpp
    src/camera/camera_shake.cpp
    src/camera/camera_update.cpp
)
target_sources(nnh_core PRIVATE ${_nnh_camera_sources})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${_nnh_camera_sources} PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
# Zoomed drawing (spec 018, D33): positions × zoom, then rounded; no fused multiply-add, so PC and 3DS put every
# zoomed draw on the same pixels.
set(_nnh_zoom_sources
    src/gfx/draw_list.cpp
    src/tilemap/tile_expand.cpp
    src/tilemap/tile_expand_zoom.cpp
    src/tilemap/visible_range.cpp
    src/tilemap/zoom_edges.cpp
)
target_sources(nnh_core PRIVATE ${_nnh_zoom_sources})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${_nnh_zoom_sources} PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
# Collision layers and queries (spec 019, D34): exact decisions in double, results rounded once to float; no fused
# multiply-add, so PC and 3DS give the same hits bit for bit. Each file is linked only by a game that calls it.
set(_nnh_query_sources
    src/collision/box_cast.cpp
    src/collision/cells_crossed.cpp
    src/collision/exact.cpp
    src/collision/grid_walk.cpp
    src/collision/grid_walk_lattice.cpp
    src/collision/hit_list.cpp
    src/collision/layers.cpp
    src/collision/point_queries.cpp
    src/collision/query_check.cpp
    src/collision/ray_box.cpp
    src/collision/raycast_boxes.cpp
    src/collision/raycast_grid.cpp
    src/collision/raycast_world.cpp
)
# The queries' only square root: -fno-math-errno makes std::sqrt one instruction (no call into libm).
set(_nnh_query_sqrt_source src/collision/ray_distance.cpp)
target_sources(nnh_core PRIVATE ${_nnh_query_sources} ${_nnh_query_sqrt_source})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${_nnh_query_sources} PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
    set_source_files_properties(${_nnh_query_sqrt_source} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-math-errno")
endif()
# Object pools (spec 020, D35): integer-only; a game that includes <nnh/pool.hpp> links only what it calls.
set(_nnh_pool_sources
    src/pool/pool_core.cpp
    src/pool/pool_runtime.cpp
    src/pool/pool_scene.cpp
)
target_sources(nnh_core PRIVATE ${_nnh_pool_sources})
# Entities, components and tags (spec 020, D35): integer-only, built on the pools' slot tables.
set(_nnh_entities_sources
    src/entities/block_layout.cpp
    src/entities/tags_find.cpp
    src/entities/world_components.cpp
    src/entities/world_create.cpp
    src/entities/world_destroy.cpp
    src/entities/world_flush.cpp
    src/entities/world_lifetime.cpp
    src/entities/world_reserve.cpp
    src/entities/world_scene.cpp
    src/entities/world_spawn.cpp
    src/entities/world_stats.cpp
    src/entities/world_tags.cpp
)
target_sources(nnh_core PRIVATE ${_nnh_entities_sources})
add_library(nnh::core ALIAS nnh_core)
target_include_directories(nnh_core PUBLIC include PRIVATE src)
target_link_libraries(nnh_core PRIVATE nnh::core_contract nnh_stb_vorbis)
target_include_directories(nnh_core PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb_vorbis)
nnh_engine_options(nnh_core)

# Build flavor (docs/architecture/foundation.md#build-flavors), PUBLIC so games and platform
# libraries see the same value through <nnh/build.hpp>:
#   Debug -> Debug, RelWithDebInfo -> Development, Release/MinSizeRel -> Release.
set(_nnh_flavor_definitions
    $<$<CONFIG:Debug>:NNH_FLAVOR_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:NNH_FLAVOR_DEVELOPMENT>
    $<$<CONFIG:Release,MinSizeRel>:NNH_FLAVOR_RELEASE>)
target_compile_definitions(nnh_core PUBLIC ${_nnh_flavor_definitions})
target_compile_definitions(nnh_core_contract INTERFACE ${_nnh_flavor_definitions})
