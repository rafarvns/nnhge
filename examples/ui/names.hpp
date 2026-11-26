#pragma once
// The example's widget keys (the game's own enum, FR-4) and their log names, and the one log line per result
// ("ui: focus title.options", "ui: value music 3", "ui: refused online", ...) that the console script expects.

#include <nnh/ui.hpp>

enum class Key : std::uint16_t {
    TitleNew, TitleContinue, TitleOptions, TitleInventory, TitleGallery, TitleQuit, TitleExtras,
    Music, Sound, Invert, Difficulty, Speed, Language, Online, Vibration, Subtitles, Hints, Autosave, Clock, Colours,
    OptionsBack, List, Grid, QuitYes, QuitNo, Dialog,
    Count
};

/// "title.options", "music", ... ("?" for an unknown key).
const char* key_name(nnh::UiKey key);

/// Logs one line per result of `ui`'s last update; `screen` names the cancelled screen.
void log_results(const nnh::Ui& ui, const char* screen);
