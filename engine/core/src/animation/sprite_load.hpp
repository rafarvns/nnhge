#pragma once
// Making a library from the bytes of a `.nnhsprite` file (spec 016 FR-16): load_sprite_library and the fuzz target use
// it.

#include <cstddef>

#include "nnh/animation.hpp"

namespace nnh::detail {

/// Validates the file, then copies it into a new block with the options' extra capacities. `name` is for messages.
Result<SpriteLibrary> sprite_library_from_file(const void* data, std::size_t size,
                                               const LoadSpriteLibraryOptions& options, const char* name);

} // namespace nnh::detail
