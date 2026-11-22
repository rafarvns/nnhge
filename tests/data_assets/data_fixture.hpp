#pragma once
// Shared by the loading tests (spec 014 stage 3): the engine on the fake platform with converted data assets
// of the tests' schema (test_types.hpp) and their index, written by the test-only writers (data_file.hpp).
//
// The standard set: enemies/bat (name only; its bytes also as enemies/e398497, which the standard index
// doesn't list), enemies/goblin (every kind set), items/ether, items/potion;
// textures art/goblin, art/potion; sound sfx/hurt; music music/battle.

#include <cstdint>

#include "data_file.hpp"
#include "nnh/data.hpp"

namespace data_test {

struct Files {
    data_file::Bytes index;
    data_file::Bytes bat, goblin, ether, potion;
};

/// The standard index spec and files (each test may change them before start()).
data_file::IndexSpec standard_index();
Files standard();

/// The schema fingerprint of the tests' Enemy / Item, as the loader computes it.
std::uint32_t enemy_fingerprint();
std::uint32_t item_fingerprint();

/// Restarts the engine (data released, fake platform reset) with `files` as assets. A file left empty is
/// still listed (an empty asset). The copies stay alive until the next start().
void start(const Files& files = standard());

/// Releases the data index and shuts the engine down.
void stop();

} // namespace data_test
