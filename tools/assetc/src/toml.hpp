#pragma once
// A bounded reader of a strict subset of TOML 1.0 for data assets (spec 014 FR-5, FR-6, FR-14; plan 014 "The
// text format"; constitution X: data files are external input). Every document it accepts is valid TOML.
//
// Accepted: UTF-8 (an optional BOM skipped), LF or CRLF, `#` comments, blank lines, `key = value` with bare
// keys [A-Za-z0-9_-]+, headers [a], [a.b], [[a]], [[a.b]] (at most 4 parts) with TOML's meaning ([a.b] under
// an array of tables names its last element). Values: true/false, decimal integers (sign, '_' between digits,
// no leading zeros), decimal floats (parsed once into a float with std::from_chars: correctly rounded, the
// same on every host), basic strings with \" \\ \n \t \uXXXX, inline arrays (may span lines, comments and a
// trailing comma allowed, nested at most 2 deep). Refused with their own message: dates and times, literal
// and multi-line strings, inline tables, dotted and quoted keys, hexadecimal/octal/binary numbers, inf/nan,
// other escapes, a key or a table set twice. Bounded: at most kMaxFileBytes, lines of at most kMaxLineBytes;
// every value takes at least one byte of text, so the document's size is linear in the file's. The reader
// stops at the first problem.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nnh::assetc::toml {

inline constexpr std::size_t kMaxFileBytes = 64 * 1024;
inline constexpr std::size_t kMaxLineBytes = 1024;
inline constexpr int kMaxHeaderParts = 4;
inline constexpr int kMaxArrayDepth = 2;

enum class Type : std::uint8_t { Bool, Integer, Float, String, Array };

struct Value {
    Type type = Type::Bool;
    int line = 0;
    bool boolean = false;
    std::int64_t integer = 0;
    float number = 0;
    bool out_of_range = false; ///< an integer outside 64 bits, or a float outside float's finite range
    std::string text;          ///< String: decoded UTF-8; Integer/Float: as written (for messages)
    std::vector<Value> items;  ///< Array
};

enum class EntryKind : std::uint8_t { Value, Table, TableArray };

/// One key of a table: a value, a sub-table (Document::table(table)) or an array of tables (elements).
struct Entry {
    std::string key;
    int line = 0; ///< where the key (or the first header naming it) is
    EntryKind kind = EntryKind::Value;
    Value value;
    int table = -1;
    std::vector<int> elements;
};

struct Table {
    std::string path;     ///< "drops" / "stats.extra" ("" for the root), for messages
    int line = 0;         ///< its header's line (0: the root, or a table only implied by a longer header)
    bool defined = false; ///< named by its own header (or the root)
    std::vector<Entry> entries;

    const Entry* find(std::string_view key) const;
};

struct Problem {
    int line = 0;
    std::string message;
};

class Document {
public:
    /// Reads `text`, replacing any earlier document. False on the first problem (problem() says where and
    /// why).
    bool parse(std::string_view text);
    const Problem& problem() const { return problem_; }

    const Table& root() const { return tables_[0]; }
    const Table& table(int index) const { return tables_[static_cast<std::size_t>(index)]; }
    int table_count() const { return static_cast<int>(tables_.size()); }

private:
    friend class Builder;
    std::vector<Table> tables_;
    Problem problem_;
};

} // namespace nnh::assetc::toml
