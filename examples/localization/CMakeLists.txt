nnh_add_game(localization
    SOURCES main.cpp bench_scene.cpp check_scene.cpp demo.cpp dialog_scene.cpp gallery_scene.cpp hud.cpp loc_scene.cpp
            options_scene.cpp self_check.cpp settings.cpp shop_scene.cpp title_scene.cpp
    ASSETS assets                  # fonts (art/make_hud_font.py, fonts/make_subset.py), strings/, art/spark.png
    LANGUAGES en pt-BR de ru ja
    BASE en
    STRICT_STRINGS                 # every warning (an untranslated key, a text over its limit) fails the build
    TITLE "NNHGE Localization"
    DESCRIPTION "Game fonts and languages: menus, dialog, shop, fonts"
    AUTHOR "NNHGE contributors")
