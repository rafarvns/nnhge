// Compile check (spec 009 FR-1): AppConfig::title defaults to the NNH_GAME_TITLE the build defines for a
// game (ctest game_title_*: with -DNNH_GAME_TITLE and EXPECTED_TITLE, and without both).
#include <string_view>

#include "nnh/app.hpp"

#ifndef EXPECTED_TITLE
#define EXPECTED_TITLE "NNHGE"
#endif

static_assert(std::string_view(nnh::AppConfig{}.title) == EXPECTED_TITLE);
