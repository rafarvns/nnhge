// A data file's structs (data_encode.hpp): keys matched with fields, required fields, nested sections,
// [[blocks]] for lists and arrays of structs, inline arrays for the other lists and arrays.

#include "data/data_format.hpp"
#include "data_encode.hpp"
#include "nnh/data.hpp"
#include "suggest.hpp"

namespace nnh::assetc {

namespace {

using detail::FieldInfo;
using detail::FieldKind;
using detail::FieldShape;
using toml::EntryKind;

std::string join(const std::string& prefix, const char* name) {
    return prefix.empty() ? name : prefix + "." + name;
}

/// How the file wrote an entry that isn't what the field needs.
std::string found_entry(const Encoder& e, const toml::Entry& entry) {
    if (entry.kind == EntryKind::Value) return found_text(entry.value);
    if (entry.kind == EntryKind::Table) return "a section ([" + e.doc.table(entry.table).path + "])";
    return "[[" + e.doc.table(entry.elements[0]).path + "]] blocks";
}

/// "5 blocks, at most 4" / "1 block, exactly 2".
std::string count_problem(const std::string& path, std::size_t n, const char* unit, const FieldInfo& f) {
    const bool list = f.shape == FieldShape::List;
    return path + ": " + std::to_string(n) + " " + unit + (n == 1 ? "" : "s") +
           (list ? ", at most " : ", exactly ") + std::to_string(f.count);
}

bool count_ok(const FieldInfo& f, std::size_t n) {
    return f.shape == FieldShape::List ? n <= f.count : n == f.count;
}

void unknown_keys(Encoder& e, const detail::TypeInfo& type, const toml::Table& table,
                  const std::string& header) {
    std::vector<std::string> names;
    for (int i = 0; i < type.field_count; ++i)
        names.emplace_back(type.fields[i].name);
    for (const toml::Entry& entry : table.entries) {
        bool known = false;
        for (const std::string& n : names)
            known = known || n == entry.key;
        if (known) continue;
        const std::string hint = closest(entry.key, names);
        e.fail(entry.line, "unknown field '" + entry.key + "'" + (header.empty() ? "" : " in " + header) +
                               (hint.empty() ? "" : " (did you mean '" + hint + "'?)"));
    }
}

/// A struct field (`toml_path`: how a header would name it).
void structs(Encoder& e, const FieldInfo& f, const toml::Entry& entry, const std::string& path,
             const std::string& toml_path, int level) {
    const detail::TypeInfo* nested = detail::find_type(e.schema.registry(), f.target);
    if (nested == nullptr || level >= nnh::data::kMaxDepth) return e.fail(entry.line, path + ": too deep");
    if (f.shape == FieldShape::One) {
        if (entry.kind != EntryKind::Table) {
            return e.fail(entry.line, path + ": expected a section ([" + toml_path + "]), found " +
                                          found_entry(e, entry));
        }
        const toml::Table& t = e.doc.table(entry.table);
        return encode_struct(e, *nested, t, path, "[" + t.path + "]", t.line > 0 ? t.line : entry.line,
                             level + 1);
    }
    if (entry.kind != EntryKind::TableArray) {
        return e.fail(entry.line,
                      path + ": expected [[" + toml_path + "]] blocks, found " + found_entry(e, entry));
    }
    const std::size_t n = entry.elements.size();
    if (!count_ok(f, n)) return e.fail(entry.line, count_problem(path, n, "block", f));
    if (f.shape == FieldShape::List) e.le(static_cast<std::uint32_t>(n), 2);
    for (std::size_t i = 0; i < n; ++i) {
        const toml::Table& t = e.doc.table(entry.elements[i]);
        encode_struct(e, *nested, t, path + "[" + std::to_string(i) + "]", "[[" + t.path + "]]", t.line,
                      level + 1);
    }
}

void values(Encoder& e, const FieldInfo& f, const toml::Entry& entry, const std::string& path) {
    if (entry.kind != EntryKind::Value) {
        const char* expected = f.shape == FieldShape::One ? expected_text(f) : "an array";
        return e.fail(entry.line, path + ": expected " + expected + ", found " + found_entry(e, entry));
    }
    const toml::Value& v = entry.value;
    if (f.shape == FieldShape::One) return encode_element(e, f, v, path);
    if (v.type != toml::Type::Array)
        return e.fail(v.line, path + ": expected an array, found " + found_text(v));
    if (!count_ok(f, v.items.size())) {
        return e.fail(v.line,
                      count_problem(path, v.items.size(), f.shape == FieldShape::List ? "item" : "value", f));
    }
    if (f.shape == FieldShape::List) e.le(static_cast<std::uint32_t>(v.items.size()), 2);
    for (std::size_t i = 0; i < v.items.size(); ++i) {
        encode_element(e, f, v.items[i], path + "[" + std::to_string(i) + "]");
    }
}

} // namespace

void encode_struct(Encoder& e, const detail::TypeInfo& type, const toml::Table& table,
                   const std::string& path, const std::string& header, int line, int level) {
    unknown_keys(e, type, table, header);
    const std::size_t count_at = e.body.size();
    e.u8(0);
    int written = 0;
    for (int i = 0; i < type.field_count; ++i) {
        const FieldInfo& f = type.fields[i];
        const toml::Entry* entry = table.find(f.name);
        if (entry == nullptr) {
            if (f.required) {
                e.fail(line,
                       (header.empty() ? "" : header + ": ") + "missing required field '" + f.name + "'");
            }
            continue;
        }
        e.u8(static_cast<std::uint32_t>(i));
        ++written;
        const std::string field_path = join(path, f.name);
        if (f.kind == FieldKind::Struct)
            structs(e, f, *entry, field_path, join(table.path, f.name), level);
        else
            values(e, f, *entry, field_path);
    }
    e.body[count_at] = static_cast<std::uint8_t>(written);
}

} // namespace nnh::assetc
