#pragma once
// Derives the data folder name from AppConfig (pure; tested in tests/app/test_data_folder.cpp).

#include <cstddef>

namespace nnh::detail {

/// Lowercases `title`, turns spaces into '-', keeps [a-z0-9_-], drops the rest; "game" if nothing is left.
/// `out` always receives a '\0'-terminated name of at most `capacity` - 1 characters.
void derive_data_folder(const char* title, char* out, std::size_t capacity);

} // namespace nnh::detail
