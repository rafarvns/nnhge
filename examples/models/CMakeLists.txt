nnh_add_game(models
    SOURCES main.cpp bench_scene.cpp check_scene.cpp probe_scenes.cpp probe_stereo.cpp self_check.cpp
            self_check_stereo.cpp showcase_draw.cpp showcase_scene.cpp smoke_scene.cpp world.cpp
    ASSETS assets                  # models/*.gltf + .bin, art/*.png (art/make_models.py)
    TITLE "NNHGE Models"
    DESCRIPTION "3D models, materials, lights and the 3D camera"
    AUTHOR "NNHGE contributors")
# The self-check's float work and the showcase's camera without fused multiply-add: the console prints the PC's bits.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(self_check.cpp self_check_stereo.cpp showcase_scene.cpp showcase_draw.cpp PROPERTIES
                                COMPILE_OPTIONS -ffp-contract=off)
endif()
