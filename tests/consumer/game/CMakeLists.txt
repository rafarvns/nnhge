# The consumer game (spec 032 FR-6, FR-16): nnh_add_game() called from a nested folder of a game project.
nnh_add_game(nnh_consumer
    SOURCES src/main.cpp src/check_scene.cpp src/self_check.cpp
    DATA_SCHEMA src/data_types.cpp
    ASSETS assets)
