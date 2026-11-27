#pragma once
// One bounded reader for the small text files nnh reads (nnh.toml, presets, the games manifest).

#include <cstddef>
#include <string>

namespace nnh::tool {

/// The bytes of `path` ("" when it can't be opened). Stops once more than `limit` bytes were read, so a caller that
/// refuses files over `limit` can tell (the result is then longer than `limit`).
std::string read_text_file(const std::string& path, std::size_t limit);

} // namespace nnh::tool
