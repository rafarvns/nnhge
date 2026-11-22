#pragma once
// A small bounded JSON reader for nnh-assetc (spec 012 T5.1, constitution X): Tiled's files are external data.
// RFC 8259 JSON, strictly: no comments, no trailing commas, no BOM, no NaN/Infinity. Bounded: the document is at
// most kMaxDocument bytes, nests at most kMaxDepth arrays/objects, holds at most kMaxValues values, a number has at
// most kMaxNumberLength characters and must fit a double; strings are valid UTF-8 (escapes decoded, surrogates
// paired). The whole document is parsed into one array of values (no per-value allocation); `Ref`s are views.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nnh::assetc::json {

inline constexpr std::size_t kMaxDocument = 64u * 1024u * 1024u;
inline constexpr int kMaxDepth = 32;
inline constexpr std::size_t kMaxValues = 8u * 1024u * 1024u; // a 1024x1024 map of 4 layers has 4 Mi cells
inline constexpr int kMaxNumberLength = 64;

/// The limits of one parse (tests lower them; the converter uses the defaults).
struct Limits {
    std::size_t max_document = kMaxDocument;
    std::size_t max_values = kMaxValues;
};

enum class Type : std::uint8_t { Null, Bool, Number, String, Array, Object };

/// One parsed value (internal layout; use Ref).
struct Node {
    double number = 0;
    std::uint32_t a = 0, b = 0;         ///< string: offset, length in the text pool; array/object: first, count
    std::uint32_t key = 0, key_length = 0; ///< member of an object: its key in the text pool
    std::uint32_t next = 0;             ///< next sibling + 1 (0 = last)
    Type type = Type::Null;
    bool boolean = false;
};

class Document;

/// A view of one value of a Document (valid while the document lives). A default Ref is "missing": every query
/// on it answers as an absent value (type Null, empty, no members), so lookups can be chained.
class Ref {
public:
    Ref() = default;
    explicit operator bool() const { return doc_ != nullptr; }

    Type type() const;
    bool is_null() const { return doc_ && type() == Type::Null; }
    bool is_bool() const { return doc_ && type() == Type::Bool; }
    bool is_number() const { return doc_ && type() == Type::Number; }
    bool is_string() const { return doc_ && type() == Type::String; }
    bool is_array() const { return doc_ && type() == Type::Array; }
    bool is_object() const { return doc_ && type() == Type::Object; }

    bool boolean() const;               ///< false unless a Bool true
    double number() const;              ///< 0 unless a Number
    std::string_view string() const;    ///< "" unless a String
    int size() const;                   ///< members of an array/object, else 0
    Ref first() const;                  ///< first member of an array/object, or missing
    Ref next() const;                   ///< next member of the same array/object, or missing
    std::string_view key() const;       ///< this member's key when it is in an object, else ""
    Ref get(std::string_view key) const; ///< the first member called `key` of an object, or missing

private:
    friend class Document;
    Ref(const Document* doc, std::uint32_t index) : doc_(doc), index_(index) {}
    const Node& node() const;
    const Document* doc_ = nullptr;
    std::uint32_t index_ = 0;
};

class Document {
public:
    /// Parses `text`, replacing any earlier document. False when the text is not valid or over a limit; then
    /// error() says where ("line 3, column 7: expected ':' after a key").
    bool parse(std::string_view text, const Limits& limits = {});
    const std::string& error() const { return error_; }
    Ref root() const { return nodes_.empty() ? Ref{} : Ref{this, 0}; }

private:
    friend class Ref;
    friend class Parser;
    std::vector<Node> nodes_;
    std::string pool_; ///< decoded strings and keys
    std::string error_;
};

} // namespace nnh::assetc::json
