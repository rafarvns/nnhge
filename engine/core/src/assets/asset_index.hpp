#pragma once
// The asset index (spec 007): the build writes the sorted list of asset paths into `.nnh_assets`; core
// reads it once at init and looks paths up case-sensitively, the same on every system. Pure; unit
// tested in tests/assets/test_asset_index.cpp.

#include <cstddef>

namespace nnh::detail {

class AssetIndex {
public:
    /// Lines that hold a path (blank lines and '\r' don't count), to size the entry storage.
    static int count_lines(const char* text, std::size_t size);

    /// Parses the `size` bytes of `text` in place — the buffer must hold `size + 1` bytes (a terminator is
    /// written) and stay alive — into `entries` (capacity `capacity`), sorted. Lines
    /// are separated by '\n'; '\r' and blank lines are ignored. @return false — and an empty index — if
    /// a line isn't a valid asset path or there are more paths than `capacity`.
    bool parse(char* text, std::size_t size, const char** entries, int capacity);

    int count() const { return count_; }
    /// The entry storage given to parse() (spec 031: the reload layer frees the indexes it parsed).
    const char** entries() const { return entries_; }

    /// The stored path equal to `path`, or nullptr.
    const char* find(const char* path) const;

    /// The stored path equal to `path` ignoring ASCII letter case (for error hints), or nullptr.
    const char* find_ignoring_case(const char* path) const;

private:
    const char** entries_ = nullptr;
    int count_ = 0;
};

} // namespace nnh::detail
