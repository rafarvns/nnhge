#pragma once
// Shared by the data converter tests (spec 014 stage 5): the engine tests' schema
// (tests/data_assets/test_types.cpp, registered in this program) written as a description and read back, and
// conversions of TOML text against it. A converted file can be checked with the engine's own validator and
// applied into the compiled struct. (Read data::List items with begin()[i]: operator[] asserts through the
// engine, which this program doesn't link.)

#include <string>
#include <string_view>
#include <vector>

#include "data_convert.hpp"
#include "diagnostics.hpp"
#include "schema_read.hpp"

namespace convert_test {

/// The tests' schema as nnh-assetc reads it (read once).
const nnh::assetc::Schema& schema();

/// A schema read from the description of `registry` (for tests with local types). Kept alive until the
/// program ends.
const nnh::assetc::Schema& schema_of(const nnh::detail::DataRegistry& registry);

struct Result {
    bool ok = false;
    std::string problems; ///< as printed, with the shown path "f"
    nnh::assetc::ConvertedData data;
};

/// Converts `text` as a data file of the type with `key`.
Result convert(std::string_view text, const char* key = "enemy", const nnh::assetc::Schema& s = schema());

bool load_compiled_raw(const std::vector<std::uint8_t>& bytes, const nnh::detail::DataRegistry& registry,
                       const void* tag, void* out);

/// Whether `bytes` passes the engine's validator against the compiled type `T` of `registry` (with its
/// compiled fingerprint), and if so, applies it into `out` (reset to T{} first).
template <class T>
bool load_compiled(const std::vector<std::uint8_t>& bytes, T& out,
                   const nnh::detail::DataRegistry& registry = nnh::detail::data_registry()) {
    return load_compiled_raw(bytes, registry, nnh::detail::type_id<T>(), &out);
}

} // namespace convert_test
