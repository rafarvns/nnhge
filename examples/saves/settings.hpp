#pragma once
// The saves example's settings (spec 027 FR-16): a second save type in its own file, settings.sav. Its
// declaration lives in settings.cpp only (the "declarations in one source" pattern): the rest of the game
// sees the struct and these two calls, never <nnh/saves.hpp>'s macros.

#include <cstdint>
#include <nnh/data.hpp>

namespace saves_example {

struct Settings {
    std::uint8_t volume = 7;   ///< 0..10
    std::uint8_t language = 0; ///< index into language_name()
    nnh::data::List<std::uint8_t, 64>
        controls; ///< an opaque blob (a real game: an ActionSet's save_to bytes)
};

inline constexpr int kLanguages = 3;
const char* language_name(int index);

/// Loads settings.sav into `s` (unchanged on any error, NotFound included: the defaults stay). Returns
/// whether it did.
bool load_settings(Settings& s);

/// Stores `s` into settings.sav. Returns whether it did (logs the error otherwise).
bool store_settings(const Settings& s);

} // namespace saves_example
