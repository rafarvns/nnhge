#pragma once
// `nnh-assetc data`'s core (spec 014 FR-5 to FR-10, FR-14; plan 014 "The text format"): one data file's TOML
// text checked against its type in the schema and written as a converted data asset `NNHD`
// (engine/core/src/data/ data_format.hpp), field by field, with the type's fingerprint — then validated with
// the engine's own validator. Every problem is reported with its line (unknown fields and enum names with a
// suggestion); a TOML syntax problem stops at the first one. The references the file makes are collected for
// the index step. Tests: tools/assetc/tests/test_data_convert*.cpp.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "diagnostics.hpp"
#include "schema_read.hpp"

namespace nnh::assetc {

/// A non-empty reference a data file makes.
struct DataReference {
    int line = 0;
    detail::FieldKind kind = detail::FieldKind::DataRef; ///< DataRef, TextureRef, SoundRef or MusicRef
    std::string target;                                  ///< DataRef: the referenced type's key; else ""
    std::string field;                                   ///< where: "drops[1].item"
    std::string path;                                    ///< the asset path, without extension
};

struct ConvertedData {
    std::vector<std::uint8_t> bytes; ///< the whole `NNHD` file
    std::vector<DataReference> references;
};

/// Converts `text`, a data file of `type` (a type of `schema`). False when there are problems (all added to
/// `problems`; `out` is then meaningless).
bool convert_data(const Schema& schema, const detail::TypeInfo& type, std::string_view text,
                  ConvertedData& out, Diagnostics& problems);

} // namespace nnh::assetc
