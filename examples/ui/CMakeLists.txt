nnh_add_game(ui
    SOURCES main.cpp bench.cpp bench_scene.cpp check_scene.cpp dialog_scene.cpp gallery_scene.cpp hand_options.cpp
            hud.cpp inventory_scene.cpp items.cpp menu_scene.cpp names.cpp options_scene.cpp self_check.cpp
            self_check_script.cpp texts.cpp themes.cpp title_scene.cpp
    ASSETS assets                  # art/ui.png (art/make_art.py)
    TITLE "NNHGE UI"
    DESCRIPTION "UI widgets: menus, options, lists, dialogs, a HUD"
    AUTHOR "NNHGE contributors")
