#pragma once
// Decoding one JSON string literal (internal to the JSON reader): escapes, surrogate pairs, UTF-8 validation.

#include <cstddef>
#include <string>
#include <string_view>

namespace nnh::assetc::json {

/// Decodes the string literal starting at text[pos] (a '"'), appending its UTF-8 bytes to `out`. On success `pos`
/// is just past the closing quote; on failure `pos` is at the offending byte and `error` names the problem.
bool decode_string(std::string_view text, std::size_t& pos, std::string& out, const char*& error);

} // namespace nnh::assetc::json
