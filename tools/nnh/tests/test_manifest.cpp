// The games manifest (manifest.hpp, spec 010).

#include "manifest.hpp"
#include "test.hpp"

using namespace nnh::tool;

NNH_TEST(manifest_lines_become_games) {
    const auto games = parse_manifest("hello|/b/examples/hello|0\r\naudio|/b/examples/audio|1\n\n");
    CHECK(games.size() == 2);
    const auto audio = find_game(games, "audio");
    CHECK(audio && audio->binary_dir == "/b/examples/audio" && audio->cia);
    const auto hello = find_game(games, "hello");
    CHECK(hello && !hello->cia);
    CHECK(!find_game(games, "sprites").has_value());
}

NNH_TEST(manifest_skips_bad_lines_and_keeps_the_last_entry) {
    const auto games = parse_manifest("bad line\nx||1\n|dir|0\ngame|/old|0\ngame|/new|1\ny|/d|2\n");
    CHECK(games.size() == 1);
    const auto game = find_game(games, "game");
    CHECK(game && game->binary_dir == "/new" && game->cia);
}
