#include "texts.hpp"

namespace {

constexpr int kTexts = static_cast<int>(Text::Count);

// UTF-8; the built-in font has Latin-1, so Portuguese accents draw.
const char* const kEnglish[kTexts] = {
    "NNHGE UI", "A choose  B back  SELECT theme", "New game", "Continue", "Options", "Inventory", "Gallery", "Quit",
    "Extras", "Quit the game?", "Yes", "No", "Options", "Music", "Sound", "Invert Y", "Difficulty", "Easy", "Normal",
    "Hard", "Text speed", "Slow", "Fast", "Language", "English", "Portugu\xC3\xAAs", "Online", "Vibration",
    "Subtitles", "Hints", "Autosave", "Clock", "Colours", "Back"};
const char* const kPortuguese[kTexts] = {
    "NNHGE UI", "A escolhe  B volta  SELECT tema", "Novo jogo", "Continuar", "Op\xC3\xA7\xC3\xB5" "es", "Invent\xC3\xA1rio",
    "Galeria", "Sair", "Extras", "Sair do jogo?", "Sim", "N\xC3\xA3o", "Op\xC3\xA7\xC3\xB5" "es", "M\xC3\xBAsica", "Som",
    "Inverter Y", "Dificuldade", "F\xC3\xA1" "cil", "Normal", "Dif\xC3\xAD" "cil", "Texto", "Lento", "R\xC3\xA1pido",
    "Idioma", "English", "Portugu\xC3\xAAs", "Online", "Vibra\xC3\xA7\xC3\xA3o", "Legendas", "Dicas", "Auto-salvar",
    "Rel\xC3\xB3gio", "Cores", "Voltar"};

const char* english(std::uint32_t key) { return key < kTexts ? kEnglish[key] : nullptr; }
const char* portuguese(std::uint32_t key) { return key < kTexts ? kPortuguese[key] : nullptr; }

} // namespace

nnh::UiResolver resolver_for(int language) { return language == 1 ? &portuguese : &english; }

const char* text_of(Text t, int language) {
    const auto k = static_cast<std::uint32_t>(t);
    const char* s = language == 1 ? portuguese(k) : english(k);
    return s != nullptr ? s : "";
}
