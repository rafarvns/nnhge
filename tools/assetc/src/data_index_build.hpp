#pragma once
// `nnh-assetc data-index`'s core (spec 014 FR-4, FR-8, FR-9, FR-12, FR-14; plan 014 "Data index"): every data
// asset's references sidecar and the game's asset list checked together, then written as the data index
// `.nnh_data` (engine/core/src/data/index_format.hpp).
//
// Checks: data asset names unique across types; every data reference names an existing data asset of the
// referenced type (else the closest name of that type is suggested); every texture reference names a `.png`
// asset and every sound or music reference a `.wav` or `.ogg` asset; no path hashes to 0 and no two paths of
// a group share a hash; the counts and the strings fit the index's limits. Problems are reported like the data
// step's — `<file>:<line>: <reason>` — for the first file (by shown path) that has any, at most 20; problems
// of the whole index (counts, media hash collisions) as `<assets folder>: <reason>`. Tests:
// tools/assetc/tests/test_data_index.cpp.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "data_refs.hpp"
#include "schema_read.hpp"

namespace nnh::assetc {

/// The 32-bit hash of an asset path (without extension). Injectable so tests can force collisions.
using PathHash = std::uint32_t (*)(std::string_view path);

/// FNV-1a 32 (core's detail::fnv1a32): the hash the engine looks references up with.
std::uint32_t fnv1a_path(std::string_view path);

struct IndexInput {
    const Schema* schema = nullptr;
    std::vector<std::string> assets; ///< every packed asset's name, with its extension
    std::vector<RefsFile> data;      ///< the sidecar of every data asset (others are ignored)
    std::string shown = "assets";    ///< the assets folder as messages show it
    PathHash hash = fnv1a_path;
};

struct IndexModel {
    struct Type {
        std::string key;
        std::uint32_t fingerprint = 0;
        std::vector<std::string> paths; ///< its data assets, in byte order
    };
    std::vector<Type> types;          ///< every keyed type of the schema, by key
    std::vector<std::string> media[3]; ///< referenced textures, sounds, music (paths without extension, sorted)
    std::uint32_t largest = 0;         ///< bytes of the largest converted data asset
};

/// Checks `in` and fills `out`. False with `errors` = the text to print (one problem per line).
bool build_index(const IndexInput& in, IndexModel& out, std::string& errors);

/// The `.nnh_data` file of `model` (FNV-1a hashes), already checked with the engine's index validator; empty
/// (and `problem` set) if that check fails, which would be a bug of this tool.
std::vector<std::uint8_t> write_index(const IndexModel& model, std::string& problem);

} // namespace nnh::assetc
