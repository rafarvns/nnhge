nnh_add_game(saves
    SOURCES main.cpp bench.cpp bench_scene.cpp check_scene.cpp difficulty_names.cpp hud.cpp progress_migrate.cpp
            progress_sum.cpp reference.cpp settings.cpp showcase_scene.cpp slot_files.cpp slots_panel.cpp tiny_game.cpp
    ASSETS assets                  # golden/v1.sav, v2.sav, v3.sav: copies of tests/data/saves (ctest saves_golden_copies_match)
    TITLE "NNHGE Saves"
    DESCRIPTION "Structured saves: versions, migrations, slots, backups"
    AUTHOR "NNHGE contributors")
