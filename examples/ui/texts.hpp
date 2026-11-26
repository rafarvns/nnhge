#pragma once
// The example's texts in two languages through the UI's resolver (FR-24's localization seam): widgets name a Text key
// (UiText::key(Text::NewGame)); switching the language and calling Ui::text_changed() re-resolves them all.

#include <nnh/ui.hpp>

enum class Text : std::uint32_t {
    GameName, Hint, NewGame, Continue, Options, Inventory, Gallery, Quit, Extras, QuitAsk, Yes, No,
    OptionsTitle, Music, Sound, InvertY, Difficulty, Easy, Normal, Hard, TextSpeed, Slow, Fast, Language, English,
    Portuguese, Online, Vibration, Subtitles, Hints, Autosave, Clock, Colours, Back, Count
};

/// The resolver for `language` (0 English, 1 Português): a plain function pointer converts to UiResolver.
nnh::UiResolver resolver_for(int language);
/// The text of a key in a language ("" for an unknown key).
const char* text_of(Text t, int language);
