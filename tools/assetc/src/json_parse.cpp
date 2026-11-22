// JSON parsing into a Document (see json.hpp): a recursive-descent parser bounded by depth, values and size.

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "json.hpp"
#include "json_string.hpp"

namespace nnh::assetc::json {

class Parser {
public:
    Parser(Document& doc, std::string_view text, const Limits& limits) : doc_(doc), text_(text), limits_(limits) {}

    bool run() {
        if (text_.size() > limits_.max_document) {
            return fail_plain("document larger than " + std::to_string(limits_.max_document >> 20) + " MiB");
        }
        std::uint32_t root = 0;
        if (!new_node(root) || !value(root, 0)) return false;
        skip_space();
        if (pos_ != text_.size()) return fail("unexpected text after the document");
        return true;
    }

private:
    Document& doc_;
    std::string_view text_;
    const Limits& limits_;
    std::size_t pos_ = 0;
    std::uint32_t line_ = 1; ///< line at pos_ (strings can't hold a raw newline, so only spaces move it)

    bool fail_plain(const std::string& what) {
        doc_.error_ = what;
        return false;
    }

    bool fail(const char* what) {
        int line = 1, column = 1;
        for (std::size_t i = 0; i < pos_ && i < text_.size(); ++i) {
            column = text_[i] == '\n' ? 1 : column + 1;
            line += text_[i] == '\n' ? 1 : 0;
        }
        return fail_plain("line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + what);
    }

    bool at_end() const { return pos_ >= text_.size(); }
    char peek() const { return at_end() ? '\0' : text_[pos_]; }

    void skip_space() {
        while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) {
            line_ += peek() == '\n' ? 1 : 0;
            ++pos_;
        }
    }

    bool new_node(std::uint32_t& index) {
        if (doc_.nodes_.size() >= limits_.max_values) return fail("too many values in the document");
        index = static_cast<std::uint32_t>(doc_.nodes_.size());
        doc_.nodes_.emplace_back();
        return true;
    }

    Node& node(std::uint32_t index) { return doc_.nodes_[index]; } // never held across new_node()

    bool literal(std::string_view word) {
        if (text_.substr(pos_, word.size()) != word) return fail("expected a value");
        pos_ += word.size();
        return true;
    }

    bool value(std::uint32_t index, int depth) {
        skip_space();
        node(index).line = line_;
        switch (peek()) {
        case '{': return container(index, depth, Type::Object);
        case '[': return container(index, depth, Type::Array);
        case '"': node(index).type = Type::String; return string(node(index).a, node(index).b);
        case 't': node(index).type = Type::Bool; node(index).boolean = true; return literal("true");
        case 'f': node(index).type = Type::Bool; return literal("false");
        case 'n': return literal("null");
        default: return number(index);
        }
    }

    /// The members of an array or object; each member is linked to the previous one.
    bool container(std::uint32_t index, int depth, Type type) {
        if (depth >= kMaxDepth) return fail("arrays and objects nested deeper than 32 levels");
        const bool object = type == Type::Object;
        const char close = object ? '}' : ']';
        node(index).type = type;
        ++pos_;
        skip_space();
        if (peek() == close) {
            ++pos_;
            return true;
        }
        std::uint32_t previous = 0, count = 0;
        for (;;) {
            std::uint32_t member = 0;
            if (!new_node(member)) return false;
            if (count == 0) node(index).a = member;
            else node(previous).next = member + 1;
            if (object && !key(member)) return false;
            if (!value(member, depth + 1)) return false;
            previous = member;
            node(index).b = ++count;
            skip_space();
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            if (peek() == close) {
                ++pos_;
                return true;
            }
            return fail(object ? "expected ',' or '}'" : "expected ',' or ']'");
        }
    }

    bool key(std::uint32_t member) {
        skip_space();
        if (peek() != '"') return fail("expected a key (a string in double quotes)");
        std::uint32_t at = 0, length = 0;
        if (!string(at, length)) return false;
        node(member).key = at;
        node(member).key_length = length;
        skip_space();
        if (peek() != ':') return fail("expected ':' after a key");
        ++pos_;
        return true;
    }

    bool number(std::uint32_t index) {
        const std::size_t start = pos_;
        auto digits = [&] {
            const std::size_t from = pos_;
            while (peek() >= '0' && peek() <= '9') ++pos_;
            return pos_ > from;
        };
        if (peek() == '-') ++pos_;
        if (peek() == '0') ++pos_;
        else if (!digits()) return fail("expected a value");
        if (peek() == '.') {
            ++pos_;
            if (!digits()) return fail("expected digits after '.'");
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') ++pos_;
            if (!digits()) return fail("expected digits in the exponent");
        }
        if (pos_ - start > static_cast<std::size_t>(kMaxNumberLength)) return fail("number too long");
        char buffer[kMaxNumberLength + 1];
        text_.copy(buffer, pos_ - start, start);
        buffer[pos_ - start] = '\0';
        const double value = std::strtod(buffer, nullptr); // the C locale: the tool never changes it
        if (!std::isfinite(value)) return fail("number out of range");
        node(index).type = Type::Number;
        node(index).number = value;
        return true;
    }

    /// A string literal, decoded at the end of the text pool.
    bool string(std::uint32_t& at, std::uint32_t& length) {
        const std::size_t start = doc_.pool_.size();
        const char* problem = "";
        if (!decode_string(text_, pos_, doc_.pool_, problem)) return fail(problem);
        at = static_cast<std::uint32_t>(start);
        length = static_cast<std::uint32_t>(doc_.pool_.size() - start);
        return true;
    }
};

bool Document::parse(std::string_view text, const Limits& limits) {
    nodes_.clear();
    pool_.clear();
    error_.clear();
    Parser parser(*this, text, limits);
    if (parser.run()) return true;
    nodes_.clear();
    pool_.clear();
    return false;
}

} // namespace nnh::assetc::json
