// No images: used when the game doesn't define embedded_images() (see image_registry.hpp). Keep this
// file alone: anything else here would be pulled in together with it.

#include "gfx/image_registry.hpp"

const nnh::detail::EmbeddedImage* nnh::detail::embedded_images(int& count) {
    count = 0;
    return nullptr;
}
