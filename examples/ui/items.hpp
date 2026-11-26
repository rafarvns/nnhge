#pragma once
// The inventory's 300 items (made up): a name, a count, an icon of the sheet and a description, from fixed tables —
// the game's own data, which the list asks for row by row (FR-11).

#include <nnh/math.hpp>

inline constexpr int kItems = 300;

const char* item_name(int index);        ///< "Potion", "Ether II", ...
int item_count(int index);               ///< 1..99
nnh::Region item_icon(int index);        ///< 16x16 in art/ui.png
const char* item_description(int index); ///< a sentence (wrapped by its label)
