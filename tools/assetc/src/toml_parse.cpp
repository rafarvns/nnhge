// The TOML reader's driver (toml.hpp): checks the text line by line, then reads it one line at a time — blank
// lines, comments, headers and `key = value` (values may span lines: arrays).

#include <cstdio>
#include <string>
#include <vector>

#include "text/utf8.hpp" // shared with core (pure file)
#include "toml_builder.hpp"

namespace nnh::assetc::toml {

namespace {

constexpr const char* kBadUtf8 = "not valid UTF-8 (or a byte order mark not at the start)";

bool check_line(Cursor& c, std::string_view line) {
    if (line.size() > kMaxLineBytes) {
        return c.fail("the line is " + std::to_string(line.size()) + " bytes: at most " +
                      std::to_string(kMaxLineBytes));
    }
    for (char ch : line) {
        const auto b = static_cast<unsigned char>(ch);
        if (b == '\r') return c.fail("a carriage return must be followed by a line feed");
        if ((b < 0x20 && b != '\t') || b == 0x7F) {
            char text[8];
            std::snprintf(text, sizeof text, "0x%02X", b);
            return c.fail(std::string("control character ") + text);
        }
    }
    if (!nnh::detail::utf8_valid(line.data(), line.size()) ||
        line.find("\xEF\xBB\xBF") != std::string_view::npos) {
        return c.fail(kBadUtf8);
    }
    return true;
}

/// Sizes, line ends, control characters and UTF-8 of the whole text, before reading it; skips a BOM.
bool check_text(Cursor& c) {
    if (c.text.size() > kMaxFileBytes) {
        // The command line reads at most 64 KiB + 1 bytes, so the real size isn't known here.
        return c.fail("the file is larger than 64 KiB");
    }
    if (c.text.substr(0, 3) == "\xEF\xBB\xBF") c.at = 3;
    for (std::size_t at = c.at; at < c.text.size(); ++c.line) {
        std::size_t end = c.text.find('\n', at);
        if (end == std::string_view::npos) end = c.text.size();
        std::string_view line = c.text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r' && end < c.text.size()) line.remove_suffix(1);
        if (!check_line(c, line)) return false;
        at = end + 1;
    }
    c.line = 1;
    return true;
}

/// A bare key at the cursor ("" when there is none).
std::string read_key(Cursor& c) {
    const std::size_t start = c.at;
    while (is_key_char(c.peek()))
        ++c.at;
    return std::string(c.text.substr(start, c.at - start));
}

bool quoted(const Cursor& c) {
    return c.peek() == '"' || c.peek() == '\'';
}

bool read_header(Cursor& c, Builder& b) {
    const int line = c.line;
    const bool array = c.peek(1) == '[';
    c.at += array ? 2 : 1;
    std::vector<std::string> parts;
    for (;;) {
        c.skip_spaces();
        if (quoted(c)) return c.fail("quoted keys aren't supported");
        std::string part = read_key(c);
        if (part.empty()) return c.fail("expected a name in the header");
        if (static_cast<int>(parts.size()) == kMaxHeaderParts) return c.fail("a header has at most 4 parts");
        parts.push_back(std::move(part));
        c.skip_spaces();
        if (c.peek() != '.') break;
        ++c.at;
    }
    if (c.peek() != ']' || (array && c.peek(1) != ']')) {
        return c.fail(array ? "expected ']]' to close the header" : "expected ']' to close the header");
    }
    c.at += array ? 2 : 1;
    if (!b.header(parts, array, line)) return false;
    c.skip_spaces();
    return c.at_line_end() || c.fail("expected a comment or the end of the line after the header");
}

bool read_key_value(Cursor& c, Builder& b) {
    const int line = c.line;
    if (quoted(c)) return c.fail("quoted keys aren't supported");
    const std::string key = read_key(c);
    if (key.empty()) return c.fail("expected a key, a [section] or a comment");
    c.skip_spaces();
    if (c.peek() == '.') return c.fail("dotted keys aren't supported: write a [" + key + "] section");
    if (c.peek() != '=') return c.fail("expected '=' after the key");
    ++c.at;
    c.skip_spaces();
    Value value;
    if (!read_value(c, value)) return false;
    if (!b.set(key, line, std::move(value))) return false;
    c.skip_spaces();
    return c.at_line_end() || c.fail("expected a comment or the end of the line after the value");
}

} // namespace

bool Document::parse(std::string_view text) {
    tables_.clear();
    Table root;
    root.defined = true;
    tables_.push_back(std::move(root));
    problem_ = Problem{};
    Cursor c{text, 0, 1, &problem_};
    if (!check_text(c)) return false;
    Builder b(*this, c);
    while (!c.done()) {
        c.skip_spaces();
        if (!c.at_line_end()) {
            const bool ok = c.peek() == '[' ? read_header(c, b) : read_key_value(c, b);
            if (!ok) return false;
        }
        c.next_line();
    }
    return true;
}

} // namespace nnh::assetc::toml
