#pragma once
// Reading a game's schema description (spec 014 FR-3, D29; the format is
// engine/core/src/data/schema_write.hpp) into registry records the shared core code works on: the
// fingerprint, the schema check and the data validator. The description comes from the game's schema host
// program; it is still read with bounds (constitution X): at most kMaxDescriptionBytes, the counts and name
// lengths of data_format.hpp, every reference resolved. Then the schema is checked like the host program
// checks it (check_schema) and every type's fingerprint must equal the one written next to it, which proves
// the program and nnh-assetc encode schemas the same way.

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "data/schema_registry.hpp"

namespace nnh::assetc {

inline constexpr std::size_t kMaxDescriptionBytes = 1024 * 1024;

/// A schema read from a description. Owns its records (stable addresses); not copyable.
class Schema {
public:
    Schema() = default;
    Schema(const Schema&) = delete;
    Schema& operator=(const Schema&) = delete;

    const detail::DataRegistry& registry() const { return registry_; }
    /// The type with this key (keyed types only), or null.
    const detail::TypeInfo* type_by_key(std::string_view key) const;
    const detail::TypeInfo* type_by_name(std::string_view name) const;
    /// The fingerprint written in the description (equal to the computed one once read).
    std::uint32_t fingerprint(const detail::TypeInfo& type) const;

private:
    friend class SchemaReader;
    detail::DataRegistry registry_;
    std::deque<std::string> strings_;
    std::deque<detail::TypeInfo> types_;
    std::deque<std::vector<detail::FieldInfo>> fields_;
    std::deque<detail::EnumInfo> enums_;
    std::deque<std::vector<std::int64_t>> enum_values_;
    std::deque<detail::TypeNode> type_nodes_;
    std::deque<detail::EnumNode> enum_nodes_;
    std::deque<std::uint32_t> fingerprints_; ///< parallel to types_
};

/// Reads `text` into `out` (which must be fresh). False with `error` = "line N: reason" or "schema: reason".
bool read_schema(std::string_view text, Schema& out, std::string& error);

} // namespace nnh::assetc
