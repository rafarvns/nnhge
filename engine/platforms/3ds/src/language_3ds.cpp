// Nintendo 3DS: the system language (contract nnh/platform/system.hpp system_language, spec 029 FR-27): the console's
// language setting and its region, read once through CFG (libctru's cfgu.h: CFG_LANGUAGE_*, CFG_REGION_*), as a
// BCP 47-style code; "en" if the service can't be reached. The region adds a country where the 3DS language list is
// ambiguous (English, French, Spanish, Portuguese).

#include <3ds.h>

#include "nnh/platform/system.hpp"

namespace nnh::platform {
namespace {

/// The code of a CFG language on a console of `region` (CFG_REGION_USA 1, EUR 2, AUS 3).
const char* code_of(u8 language, u8 region) {
    const bool usa = region == CFG_REGION_USA;
    const bool eur = region == CFG_REGION_EUR || region == CFG_REGION_AUS;
    switch (language) {
    case CFG_LANGUAGE_JP: return "ja";
    case CFG_LANGUAGE_EN: return usa ? "en-US" : (eur ? "en-GB" : "en");
    case CFG_LANGUAGE_FR: return usa ? "fr-CA" : (region == CFG_REGION_EUR ? "fr-FR" : "fr");
    case CFG_LANGUAGE_DE: return "de";
    case CFG_LANGUAGE_IT: return "it";
    case CFG_LANGUAGE_ES: return usa ? "es-419" : (region == CFG_REGION_EUR ? "es-ES" : "es");
    case CFG_LANGUAGE_ZH: return "zh-Hans";
    case CFG_LANGUAGE_KO: return "ko";
    case CFG_LANGUAGE_NL: return "nl";
    case CFG_LANGUAGE_PT: return usa ? "pt-BR" : (region == CFG_REGION_EUR ? "pt-PT" : "pt");
    case CFG_LANGUAGE_RU: return "ru";
    case CFG_LANGUAGE_TW: return "zh-Hant";
    default: return "en";
    }
}

} // namespace

const char* system_language() {
    static const char* code = nullptr;
    if (code != nullptr) return code;
    code = "en";
    if (R_SUCCEEDED(cfguInit())) {
        u8 language = 0, region = 0;
        if (R_SUCCEEDED(CFGU_GetSystemLanguage(&language)) && R_SUCCEEDED(CFGU_SecureInfoGetRegion(&region))) {
            code = code_of(language, region);
        }
        cfguExit();
    }
    return code;
}

} // namespace nnh::platform
