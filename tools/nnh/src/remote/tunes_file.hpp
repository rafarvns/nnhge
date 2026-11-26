#pragma once
// The `--tunes FILE` reader (spec 030 FR-15, plan "--tunes FILE"): a dump (TOML tables per group, `leaf = value`,
// comments) or quoted full names (`"player/jump" = 5.25`), read before anything is sent; errors as `file:line: reason`.
// Bounded (1 MiB, 4096 values). Pure; tested (and mini-fuzzed) in tools/nnh/tests/test_tunes_file.cpp.

#include <string>
#include <string_view>
#include <vector>

namespace nnh::tool::remote {

inline constexpr std::size_t kMaxTunesBytes = 1u << 20;
inline constexpr std::size_t kMaxTunesValues = 4096;

struct TuneValue {
    std::string name;  ///< full name (`player/jump`)
    std::string value; ///< the value's text, unquoted (`5.25`, `true`, `#FFC040FF`, `fire`)
    int line = 0;
};

struct TunesFile {
    std::vector<TuneValue> values;
    std::vector<std::string> errors; ///< "file:line: reason"; empty = ok
};

TunesFile read_tunes(std::string_view text, const std::string& file);

} // namespace nnh::tool::remote
