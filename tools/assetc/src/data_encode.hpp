#pragma once
// Writing a data file's values field by field (data_convert.hpp), shared by the struct walk (data_tables.cpp)
// and the values of each kind (data_values.cpp). Internal to the converter. Problems go to the Diagnostics
// with their lines; the bytes written are then meaningless (convert_data drops them).

#include <cstdint>
#include <string>
#include <vector>

#include "data_convert.hpp"
#include "toml.hpp"

namespace nnh::assetc {

struct Encoder {
    const Schema& schema;
    Diagnostics& problems;
    const toml::Document& doc;
    std::vector<std::uint8_t> body;
    std::vector<DataReference> references;

    void u8(std::uint32_t v) { body.push_back(static_cast<std::uint8_t>(v)); }
    void le(std::uint32_t v, int width) {
        for (int i = 0; i < width; ++i)
            u8(v >> (8 * i));
    }
    void fail(int line, std::string message) { problems.add(line, std::move(message)); }
};

/// A struct value: `table`'s keys against `type`'s fields. `path` names the struct in messages ("" for the
/// root, "stats", "drops[1]"); `header` is how the file wrote it ("" for the root, "[stats]", "[[drops]]");
/// `line` is where (for missing fields).
void encode_struct(Encoder& e, const detail::TypeInfo& type, const toml::Table& table,
                   const std::string& path, const std::string& header, int line, int level);

/// One value of field `f` (any kind but Struct), named `path` in messages.
void encode_element(Encoder& e, const detail::FieldInfo& f, const toml::Value& v, const std::string& path);

/// "an integer", "true or false"... for "expected X, found Y".
const char* expected_text(const detail::FieldInfo& f);
const char* found_text(const toml::Value& v);

} // namespace nnh::assetc
