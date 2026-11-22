#pragma once
// The converted data asset `NNHD` around a body of values (spec 014 FR-10;
// engine/core/src/data/data_format.hpp): header, CRC-32, the type's fingerprint — then checked with the
// engine's own validator (D27: the converter never writes a file the console would refuse). Tests:
// tools/assetc/tests/test_data_writer.cpp.

#include <cstdint>
#include <string>
#include <vector>

#include "schema_read.hpp"

namespace nnh::assetc {

/// `file` = header + `body` for an asset of `type`. False, with `problem`, when it is over 64 KiB or the
/// engine's validator refuses it (a bug in nnh-assetc).
bool write_data(const Schema& schema, const detail::TypeInfo& type, const std::vector<std::uint8_t>& body,
                std::vector<std::uint8_t>& file, std::string& problem);

} // namespace nnh::assetc
