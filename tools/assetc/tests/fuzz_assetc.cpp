// Fuzz target bodies of nnh-assetc (see fuzz_assetc.hpp).

#include "fuzz_assetc.hpp"

#include <cstdlib>
#include <string>
#include <string_view>

#include "convert.hpp"
#include "files.hpp"
#include "json.hpp"
#include "tilemap/map_validate.hpp"
#include "toml.hpp"

namespace nnh::assetc::fuzz {

namespace {

/// Touches every value once (a walk bounded by the document: each value is visited once).
std::size_t walk(json::Ref v, int depth) {
    std::size_t seen = 1 + v.string().size() + v.key().size();
    if (v.is_number() && v.number() != v.number()) seen += 1; // NaN can't come out of the reader
    for (json::Ref m = v.first(); m; m = m.next()) {
        if (v.is_object() && !v.get(m.key())) seen += 1000000; // every key can be looked up
        seen += walk(m, depth + 1);
    }
    return seen;
}

std::size_t walk_value(const toml::Value& v, int depth) {
    if (depth > toml::kMaxArrayDepth) std::abort(); // arrays nest at most 2 deep
    std::size_t seen = 1 + v.text.size();
    for (const toml::Value& item : v.items) seen += walk_value(item, depth + 1);
    return seen;
}

/// Every table is reached once from the root (through tables and arrays of tables), and each lookup by key finds
/// its own entry.
std::size_t walk_table(const toml::Document& doc, const toml::Table& t, int depth) {
    if (depth > toml::kMaxHeaderParts) std::abort();
    std::size_t seen = 1;
    for (const toml::Entry& e : t.entries) {
        if (t.find(e.key) != &e || e.line < 1) std::abort();
        if (e.kind == toml::EntryKind::Value) seen += walk_value(e.value, 0);
        else if (e.kind == toml::EntryKind::Table) seen += walk_table(doc, doc.table(e.table), depth + 1);
        else {
            for (int element : e.elements) seen += walk_table(doc, doc.table(element), depth + 1);
        }
    }
    return seen;
}

} // namespace

int toml_target(const std::uint8_t* data, std::size_t size) {
    toml::Document doc;
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    if (doc.parse(text)) {
        if (walk_table(doc, doc.root(), 0) > 2 * size + 2) std::abort(); // linear in the text
    } else if (doc.problem().line < 1 || doc.problem().message.empty()) {
        std::abort(); // every refusal says where and why
    }
    return 0;
}

int json_target(const std::uint8_t* data, std::size_t size) {
    json::Document doc;
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    if (doc.parse(text)) {
        if (walk(doc.root(), 0) >= 1000000) std::abort();
    } else if (doc.error().empty()) {
        std::abort(); // every refusal says why
    }
    return 0;
}

int map_target(const std::uint8_t* data, std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    const std::size_t split = input.find('\0');
    MemoryFiles files;
    files.add("/g/assets/levels/map.tmj", std::string(input.substr(0, split)));
    if (split != std::string_view::npos) files.add("/g/assets/tiles/set.tsj", std::string(input.substr(split + 1)));
    // A 64x64 PNG header (the converter reads only the size).
    static const char kPng[] = "\x89PNG\r\n\x1a\n\0\0\0\x0DIHDR\0\0\0\x40\0\0\0\x40\x08\x06\0\0\0";
    files.add("/g/assets/tiles/set.png", std::string(kPng, sizeof kPng - 1));
    Converted out;
    Problem problem;
    if (convert_map(files, "/g/assets/levels/map.tmj", "/g/assets", out, problem)) {
        if (!nnh::detail::validate_map(out.bytes.data(), out.bytes.size()).ok()) std::abort();
    } else if (problem.file.empty() || problem.reason.empty()) {
        std::abort(); // every refusal names a file and a reason
    }
    return 0;
}

} // namespace nnh::assetc::fuzz
