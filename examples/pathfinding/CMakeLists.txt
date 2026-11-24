nnh_add_game(pathfinding
    SOURCES main.cpp bench.cpp bench_scene.cpp check_maps.cpp check_scene.cpp crowd.cpp debug_view.cpp hero.cpp hud.cpp
            level.cpp minimap.cpp self_check.cpp showcase_scene.cpp
    ASSETS assets                  # levels/keep.tmj (Tiled, made by art/make_level.py), art/tiles.png (art/make_art.py)
    TITLE "NNHGE Pathfinding"
    DESCRIPTION "Pathfinding: A*, smoothing, distance fields, doors"
    AUTHOR "NNHGE contributors")
