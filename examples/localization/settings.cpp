#include "settings.hpp"

#include <cstdio>
#include <nnh/log.hpp>
#include <nnh/saves.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

namespace {

struct LanguageSettings {
    nnh::data::String<15> language; ///< a shipped code; empty: follow the system language
};
NNH_SAVE_TYPE(LanguageSettings, "settings", NNH_SAVE_FIELD(language, 1));

constexpr const char* kFile = "localization.sav";
nnh::save::Buffer<LanguageSettings> g_buffer; // static: never on the stack

} // namespace

bool load_language_choice(char* out, int capacity) {
    LanguageSettings s;
    if (!nnh::save::load(kFile, s, g_buffer)) return false;
    std::snprintf(out, static_cast<std::size_t>(capacity), "%s", s.language.c_str());
    return true;
}

bool store_language_choice(const char* code) {
    LanguageSettings s;
    s.language.assign(code);
    auto r = nnh::save::store(kFile, s, g_buffer);
    if (!r) NNH_LOG_ERROR(example, "loc: language not saved: %s", r.error().message);
    else NNH_LOG_INFO(example, "loc: saved language %s", code[0] != 0 ? code : "(system)");
    return r.ok();
}
