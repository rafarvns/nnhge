// Data folder name derived from the title.

#include <cstring>

#include "app/data_folder.hpp"
#include "test.hpp"

namespace {

bool derives(const char* title, const char* expected) {
    char out[32];
    nnh::detail::derive_data_folder(title, out, sizeof out);
    return std::strcmp(out, expected) == 0;
}

} // namespace

NNH_TEST(title_becomes_a_safe_lowercase_folder_name) {
    CHECK(derives("Hello NNHGE", "hello-nnhge"));
    CHECK(derives("Super Game 2!", "super-game-2"));
    CHECK(derives("../../evil", "evil"));
    CHECK(derives("sdmc:/x", "sdmcx"));
}

NNH_TEST(empty_or_unusable_titles_fall_back_to_game) {
    CHECK(derives("", "game"));
    CHECK(derives("!!!", "game"));
    CHECK(derives(nullptr, "game"));
}

NNH_TEST(long_titles_are_cut_to_the_buffer) {
    char out[8];
    nnh::detail::derive_data_folder("abcdefghijklmnop", out, sizeof out);
    CHECK(std::strcmp(out, "abcdefg") == 0);
}
