# The consumer game (spec 032 FR-6, FR-16): nnh_add_game() called from a nested folder of a game project, with every
# keyword it accepts. assets/ holds one asset of each kind the build converts (an image, a Tiled map and its tileset, an
# Aseprite export, a data file of DATA_SCHEMA's type, a font over a TTF, two string files, a glTF model, a sound), copied
# from the engine's test fixtures (tests/data/).
nnh_add_game(nnh_consumer
    SOURCES src/main.cpp src/check_scene.cpp src/self_check.cpp
    ASSETS assets
    IMAGES images/logo.png
    DATA_SCHEMA src/data_types.cpp
    LANGUAGES en pt-BR
    BASE en
    TITLE "NNHGE Consumer"
    DESCRIPTION "A game project outside the engine"
    AUTHOR "NNHGE contributors"
    VERSION 1.2.3
    ICON package/icon.png
    CIA UNIQUE_ID 0xF7032 PRODUCT_CODE CTR-N-NNHC BANNER package/banner.png BANNER_AUDIO package/banner.wav)
