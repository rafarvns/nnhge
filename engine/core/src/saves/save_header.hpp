#pragma once
// The structured header at the start of a structured save's payload (spec 027, D45). Pure; tested in
// tests/saves/test_header.cpp.
//
//   0 u32 magic "NNSV"   4 u8 format (1)   5 u8 key length (1..16)   6 u16 flags (0)
//   8 u32 game save version (>= 1)   12 u32 key hash (FNV-1a 32 of the key)   16 key bytes (a-z 0-9 _)

#include <cstddef>
#include <cstdint>

#include "nnh/result.hpp"
#include "nnh/saves.hpp"

namespace nnh::detail {

struct SaveStructHeader {
    std::uint32_t version = 0;  ///< the game's save version that wrote the file
    std::uint32_t key_hash = 0;
    std::uint8_t format = 0;
    std::uint8_t key_length = 0;
    char key[save::kMaxKeyLength + 1] = {};
    std::size_t size = 0;       ///< header bytes (16 + key length): where the root's fields start
};

/// True for 1-16 characters of a-z 0-9 _ starting with a letter.
bool save_key_valid(const char* key);

/// Writes the header for `key` (valid) and `version` at `out` (room for 16 + key length bytes); returns its size.
std::size_t build_save_struct_header(std::uint8_t* out, const char* key, std::uint32_t version);

/// Parses and checks the header at the start of a payload. Errors: Unsupported for a payload without the
/// structured magic (a raw nnh::save file: "a raw nnh::save file") or of a newer format ("format 2; this engine
/// reads 1"), Corrupted for every other fault (named). Doesn't look past the key.
Result<SaveStructHeader> parse_save_struct_header(const std::uint8_t* data, std::size_t size);

} // namespace nnh::detail
