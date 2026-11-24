nnh_add_game(prefabs
    SOURCES main.cpp bench.cpp bench_scene.cpp hud.cpp room.cpp room_draw.cpp room_prefabs.cpp roster.cpp self_check.cpp
            self_check_defs.cpp self_check_scene.cpp self_check_sides.cpp showcase_scene.cpp
    DATA_SCHEMA data_types.cpp     # EnemyData (spec 014): one prefab per assets/enemies/*.enemy (roster.cpp)
    ASSETS assets                  # rooms/room.tmj (Tiled, made by art/make_room.py), tiles/room.png, enemies/*.enemy
    TITLE "NNHGE Prefabs"
    DESCRIPTION "Prefabs: libraries, variants, overrides, Tiled objects"
    AUTHOR "NNHGE contributors")
