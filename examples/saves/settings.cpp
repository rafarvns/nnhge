// The saves example's settings (settings.hpp): the save declaration and the buffer live in this one source.

#include "settings.hpp"

#include "demos.hpp"

#include <nnh/saves.hpp>

namespace saves_example {

NNH_SAVE_TYPE(Settings, "settings", NNH_SAVE_FIELD(volume, 1), NNH_SAVE_FIELD(language, 2),
              NNH_SAVE_FIELD(controls, 3));

namespace {

constexpr const char* kFile = "settings.sav";
nnh::save::Buffer<Settings> g_buffer; // 90 bytes: static is fine

} // namespace

const char* language_name(int index) {
    static constexpr const char* kNames[kLanguages] = {"English", "Portugues", "Espanol"};
    return index >= 0 && index < kLanguages ? kNames[index] : "?";
}

bool load_settings(Settings& s) {
    return nnh::save::load(kFile, s, g_buffer).ok();
}

bool store_settings(const Settings& s) {
    const auto r = nnh::save::store(kFile, s, g_buffer);
    if (!r) NNH_LOG_WARN(example, "settings: %s", r.error().message);
    return r.ok();
}

} // namespace saves_example
