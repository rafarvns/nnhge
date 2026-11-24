nnh_add_game(particles
    SOURCES main.cpp bench.cpp bench_scene.cpp check_scene.cpp effects.cpp gallery.cpp presets.cpp self_check.cpp
            showcase_scene.cpp touch_panel.cpp
    DATA_SCHEMA data_types.cpp     # FxPreset: assets/fx/*.particles (spec 014; the preset pattern of spec 024)
    ASSETS assets                  # art/particles.png (art/make_art.py), fx/*.particles
    TITLE "NNHGE Particles"
    DESCRIPTION "Particles: emitters, bursts, trails, presets"
    AUTHOR "NNHGE contributors")
# The self-check's few float operations: no fused multiply-add, like the engine's (its golden is the same everywhere).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(self_check.cpp PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
