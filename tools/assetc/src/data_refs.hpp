#pragma once
// The references sidecar `nnh_data/refs/<asset>.refs` (spec 014, plan 014 "Build integration"): what one data
// file refers to, for the data-index step that checks references across files (stage 6). Written for every
// asset the data step handles — a copied asset gets an empty one, so the build always has the output it
// expects.
//
// Format, version 1 — ASCII, LF, one record per line:
//   nnh-data-refs 1
//   asset <key> <converted size> <name>                      (absent for an asset that isn't a data asset)
//   ref <line> <data|texture|sound|music> <target key or -> <field> <path>
// The asset name and the path come last because asset paths may contain spaces. Tests:
// tools/assetc/tests/test_data_writer.cpp.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "data_convert.hpp"

namespace nnh::assetc {

inline constexpr const char* kRefsHeader = "nnh-data-refs 1\n";

/// The sidecar of the converted data asset `name` (its path without extension) of type `key`, `size` bytes.
std::string refs_text(std::string_view name, std::string_view key, std::size_t size,
                      const std::vector<DataReference>& references);

/// The sidecar of an asset that isn't a data asset (copied unchanged).
std::string empty_refs_text();

} // namespace nnh::assetc
