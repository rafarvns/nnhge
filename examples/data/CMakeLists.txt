nnh_add_game(data
    SOURCES main.cpp bestiary_scene.cpp check.cpp check_scene.cpp
    DATA_SCHEMA data_types.cpp     # compiled into the game and into the build's schema program (spec 014)
    ASSETS assets                  # *.enemy, *.item, *.settings: data assets; art, sounds, music: media
    TITLE "NNHGE Data"
    DESCRIPTION "Data assets: a bestiary and shop"
    AUTHOR "NNHGE contributors")
