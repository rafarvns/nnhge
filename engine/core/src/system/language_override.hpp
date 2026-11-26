#pragma once
// The remote's language override, read by system_language.cpp (language_override.cpp). Internal.

namespace nnh::detail {

/// The code a test script set, or nullptr.
const char* language_override();

} // namespace nnh::detail
