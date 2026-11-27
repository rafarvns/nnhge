nnh_add_game(reload
    SOURCES main.cpp bench_scene.cpp reload_scene.cpp self_check.cpp world.cpp
    DATA_SCHEMA data_types.cpp     # enemies/*.enemy
    ASSETS assets                  # art, levels, sfx, models (art/make_assets.py), enemies, strings
    LANGUAGES en pt-BR
    BASE en
    TITLE "NNHGE Reload"
    DESCRIPTION "Hot reload of assets: edit and save while the game runs"
    AUTHOR "NNHGE contributors")
