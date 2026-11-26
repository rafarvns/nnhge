#pragma once
// The player's language choice, kept with spec 027's saves (`localization.sav`): the code chosen in Options, or empty
// while the game follows the system language. Read before loc::start, written when the player picks a language.

/// The saved choice into `out` (capacity 16): a shipped code, or "" (follow the system). False when there is no save.
bool load_language_choice(char* out, int capacity);
/// Saves the player's choice (`code`), or "" to follow the system language. False when the save failed (logged).
bool store_language_choice(const char* code);
