#pragma once
// The TOML reader's position in the text (toml.hpp): the byte, the line, and the first problem. Internal to
// the reader (toml_parse.cpp, toml_value.cpp). The text was checked line by line before (sizes, line ends,
// control characters, UTF-8), so a line end is always "\n" or "\r\n".

#include <cstddef>
#include <string>
#include <string_view>

#include "toml.hpp"

namespace nnh::assetc::toml {

struct Cursor {
    std::string_view text;
    std::size_t at = 0;
    int line = 1;
    Problem* problem = nullptr;

    bool done() const { return at >= text.size(); }
    char peek(std::size_t ahead = 0) const { return at + ahead < text.size() ? text[at + ahead] : '\0'; }

    /// False, recording `message` at the current line.
    bool fail(std::string message) const {
        problem->line = line;
        problem->message = std::move(message);
        return false;
    }

    void skip_spaces() {
        while (peek() == ' ' || peek() == '\t')
            ++at;
    }
    /// At a line end, a comment or the end of the text.
    bool at_line_end() const { return done() || peek() == '\n' || peek() == '\r' || peek() == '#'; }
    /// Past a comment (if any) and the line end (if any).
    void next_line() {
        while (!done() && peek() != '\n')
            ++at;
        if (!done()) {
            ++at;
            ++line;
        }
    }
    /// Past spaces, comments and line ends (inside arrays).
    void skip_blank() {
        for (;;) {
            skip_spaces();
            if (done() || !at_line_end()) return;
            next_line();
        }
    }
};

inline bool is_key_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

/// Reads one value at the cursor (arrays nested `depth` deep so far). False with the cursor's problem set.
bool read_value(Cursor& c, Value& out, int depth = 0);

} // namespace nnh::assetc::toml
