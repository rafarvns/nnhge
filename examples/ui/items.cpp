#include "items.hpp"

namespace {

const char* const kNames[30] = {
    "Potion",       "Ether",       "Elixir",      "Antidote",   "Torch",        "Rope",    "Key",     "Map",
    "Lantern",      "Compass",     "Herb",        "Bread",      "Apple",        "Cheese",  "Fish",    "Crystal",
    "Feather",      "Bell",        "Scroll",      "Book",       "Ring",         "Amulet",  "Shield",  "Helmet",
    "Boots",        "Gloves",      "Cape",        "Lens",       "Seed",         "Shell"};
const char* const kGrades[10] = {"", " II", " III", " IV", " V", " VI", " VII", " VIII", " IX", " X"};
const char* const kText[6] = {
    "A small thing found on the road north of the village. It may help later.",
    "Merchants pay well for these, but a careful traveller keeps one or two.",
    "Old and worn, yet it still works. Use it when nothing else will do.",
    "It hums softly at dawn. Nobody in the valley knows where it came from.",
    "Light to carry and hard to break: a favourite of the river guides.",
    "The label is faded. Try it in a quiet place, just in case."};

char g_names[kItems][16];
bool g_ready = false;

void prepare() {
    for (int i = 0; i < kItems; ++i) {
        const char* base = kNames[i % 30];
        const char* grade = kGrades[i / 30];
        int n = 0;
        for (const char* p = base; *p != 0 && n < 15; ++p) g_names[i][n++] = *p;
        for (const char* p = grade; *p != 0 && n < 15; ++p) g_names[i][n++] = *p;
        g_names[i][n] = 0;
    }
    g_ready = true;
}

} // namespace

const char* item_name(int index) {
    if (!g_ready) prepare();
    return index >= 0 && index < kItems ? g_names[index] : "";
}

int item_count(int index) { return 1 + (index * 37 + 11) % 99; }

nnh::Region item_icon(int index) {
    const int k = (index * 7) % 16;
    return {(k % 8) * 16, 40 + (k / 8) * 16, 16, 16};
}

const char* item_description(int index) { return kText[(index * 5 + 1) % 6]; }
