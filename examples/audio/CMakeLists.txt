nnh_add_game(audio
    SOURCES main.cpp
    ASSETS assets
    TITLE "NNHGE Audio"
    DESCRIPTION "Sound effects, streamed music and the mixer"
    AUTHOR "NNHGE contributors"
    VERSION 1.0.0
    ICON icon.png                              # art from make_art.py
    CIA UNIQUE_ID 0xE0A01                      # an installable .cia: cmake --build ... --target audio_cia
        BANNER banner.png BANNER_AUDIO banner.wav)
