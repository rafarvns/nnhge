# nnh::core — the portable engine. Never depends on a system SDK.
# Sources are listed explicitly (no file(GLOB)); see docs/tech/cmake.md.

# Platform contract: what every platform library implements and nnh::core calls.
add_library(nnh_core_contract INTERFACE)
add_library(nnh::core_contract ALIAS nnh_core_contract)
target_include_directories(nnh_core_contract INTERFACE contract include)

add_library(nnh_core STATIC
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
    src/gfx/draw_list.cpp
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
    src/tilemap/map_copy.cpp
    src/tilemap/map_draw.cpp
    src/tilemap/map_layout.cpp
    src/tilemap/map_load.cpp
    src/tilemap/map_objects.cpp
    src/tilemap/map_properties.cpp
    src/tilemap/map_property_table.cpp
    src/tilemap/map_strings.cpp
    src/tilemap/map_tiles.cpp
    src/tilemap/map_validate.cpp
    src/tilemap/map_validate_content.cpp
    src/tilemap/tile_animation.cpp
    src/tilemap/tile_expand.cpp
    src/tilemap/tilemap.cpp
    src/tilemap/visible_range.cpp
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
    src/actions/source_eval.cpp
    src/actions/source_names.cpp
)
target_sources(nnh_core PRIVATE ${_nnh_actions_sources})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${_nnh_actions_sources} PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
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
