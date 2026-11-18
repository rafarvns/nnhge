#pragma once
// The images a game embeds with nnh_add_game(... IMAGES ...). Internal.
//
// The build generates a file defining embedded_images() for each game. Games without images (and
// tools) link the empty default from image_registry_default.cpp: it lives alone in its object file, so
// the static library only pulls it in when the game didn't define its own. Tests define their own.

#include <cstddef>

namespace nnh::detail {

struct EmbeddedImage {
    const char* name;  ///< file name without folder and extension
    const void* data;  ///< in the platform's format (3DS: .t3x, PC: PNG)
    std::size_t size;
};

/// The game's images; `count` receives how many.
const EmbeddedImage* embedded_images(int& count);

} // namespace nnh::detail
