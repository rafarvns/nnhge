// Reading string files (strings_read.hpp).

#include "strings_read.hpp"

#include <cstdio>

#include "text/utf8.hpp"

namespace nnh::assetc {
namespace {

bool name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

/// A dotted name of [A-Za-z0-9_-] parts.
bool dotted(std::string_view s) {
    if (s.empty() || s.size() > 128 || s.front() == '.' || s.back() == '.') return false;
    char prev = '.';
    for (const char c : s) {
        if (c == '.' ? prev == '.' : !name_char(c)) return false;
        prev = c;
    }
    return true;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

void append_utf8(std::string& out, std::uint32_t c) {
    if (c < 0x80) out += static_cast<char>(c);
    else if (c < 0x800) out += static_cast<char>(0xC0 | (c >> 6)), out += static_cast<char>(0x80 | (c & 0x3F));
    else if (c < 0x10000)
        out += static_cast<char>(0xE0 | (c >> 12)), out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)),
            out += static_cast<char>(0x80 | (c & 0x3F));
    else
        out += static_cast<char>(0xF0 | (c >> 18)), out += static_cast<char>(0x80 | ((c >> 12) & 0x3F)),
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)), out += static_cast<char>(0x80 | (c & 0x3F));
}

/// Decodes the escapes of a basic string's body. @return "" or the problem.
std::string unescape(std::string_view body, std::string& out) {
    for (std::size_t i = 0; i < body.size(); ++i) {
        const char c = body[i];
        if (c != '\\') {
            out += c;
            continue;
        }
        if (++i >= body.size()) return "a string ends with a lone '\\'";
        switch (body[i]) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'u': {
            std::uint32_t v = 0;
            for (int k = 1; k <= 4; ++k) {
                const char h = i + static_cast<std::size_t>(k) < body.size() ? body[i + static_cast<std::size_t>(k)] : 'x';
                const int d = h >= '0' && h <= '9' ? h - '0' : (h >= 'A' && h <= 'F' ? h - 'A' + 10 : (h >= 'a' && h <= 'f' ? h - 'a' + 10 : -1));
                if (d < 0) return "\\u needs 4 hexadecimal digits";
                v = v * 16 + static_cast<std::uint32_t>(d);
            }
            if (v >= 0xD800 && v <= 0xDFFF) return "\\u of a surrogate";
            append_utf8(out, v);
            i += 4;
            break;
        }
        default: return std::string("unknown escape \\") + body[i];
        }
    }
    return "";
}

struct Parser {
    const std::string& file;
    StringsLanguage& lang;
    std::vector<std::string>& problems;
    std::string table;  // the current header
    bool ok = true;
    void fail(int line, const std::string& what) {
        problems.push_back(file + ":" + std::to_string(line) + ": " + what);
        ok = false;
    }
    void set_key(const std::string& name, const std::string& value, int line);
    void set_meta(const std::string& key, std::string_view raw, const std::string& value, bool is_string, int line);
};

void Parser::set_key(const std::string& full, const std::string& value, int line) {
    const std::size_t dot = full.rfind('.');
    const std::string last = dot == std::string::npos ? full : full.substr(dot + 1);
    const int category = category_index(last);
    const bool form = dot != std::string::npos && (category >= 0 || last == "plural");
    const std::string name = form ? full.substr(0, dot) : full;
    StringKey& k = lang.keys[name];
    if (k.where.line == 0) k.where = {file, line};
    if (!form) {
        if (k.has_text || k.plural()) {
            fail(line, "\"" + name + "\" is set twice (first at line " + std::to_string(k.where.line) + ")");
            return;
        }
        k.text = value;
        k.has_text = true;
        return;
    }
    if (last == "plural") {
        k.plural_arg = value;
        return;
    }
    if ((k.form_bits & (1u << category)) != 0 || k.has_text) {
        fail(line, "\"" + full + "\" is set twice (first at line " + std::to_string(k.where.line) + ")");
        return;
    }
    k.form_bits = static_cast<std::uint8_t>(k.form_bits | (1u << category));
    k.forms[category] = value;
}

void Parser::set_meta(const std::string& key, std::string_view raw, const std::string& value, bool is_string, int line) {
    LanguageMeta& m = lang.meta;
    const auto integer = [&](int lo, int hi, int& out) {
        int v = 0;
        if (is_string || raw.empty() || raw.size() > 9) return fail(line, key + " is a whole number");
        for (const char c : raw) {
            if (c < '0' || c > '9') return fail(line, key + " is a whole number");
            v = v * 10 + (c - '0');
        }
        if (v < lo || v > hi) return fail(line, key + " is " + std::to_string(lo) + " to " + std::to_string(hi));
        out = v;
    };
    if (table == "language") {
        if (key == "name" && is_string) m.name = value, m.name_at = {file, line};
        else if (key == "fallback" && is_string) m.fallback = value, m.fallback_at = {file, line};
        else if (key == "grouping" && is_string) m.grouping = value;
        else if (key == "grouping_min") integer(1, 9, m.grouping_min);
        else fail(line, "unknown [language] key \"" + key + "\" (name, fallback, grouping, grouping_min)");
    } else if (table == "language.fonts") {
        if (!is_string) return fail(line, "a font role names a font asset (a string)");
        m.fonts.emplace_back(key, value);
        if (m.fonts_at.line == 0) m.fonts_at = {file, line};
    } else if (table == "language.plural") {
        if (!is_string) return fail(line, "a plural category's rule is CLDR rule text (a string)");
        m.plural.push_back({key, value});
    } else if (table == "language.limits") {
        int v = 0;
        integer(1, 4096, v);
        m.limits.emplace_back(key, v);
    } else {
        fail(line, "unknown table [" + table + "] (language, language.fonts, language.plural, language.limits)");
    }
}

} // namespace

bool read_strings(std::string_view text, const std::string& file, StringsLanguage& into, std::vector<std::string>& problems) {
    Parser p{file, into, problems, {}};
    if (text.size() > kMaxStringsFileBytes) {
        p.fail(1, "larger than 1 MiB");
        return false;
    }
    if (text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);
    if (!nnh::detail::utf8_valid(text.data(), text.size())) {
        std::size_t line = 1, col = 1, bad = 0; // where: the first byte that isn't valid UTF-8
        for (std::size_t i = 0; i < text.size(); ++i) {
            std::size_t len = 1;
            const auto b = static_cast<unsigned char>(text[i]);
            if (b >= 0xF0) len = 4;
            else if (b >= 0xE0) len = 3;
            else if (b >= 0xC0) len = 2;
            if (i + len > text.size() || !nnh::detail::utf8_valid(text.data() + i, len) || (b >= 0x80 && b < 0xC0)) {
                bad = i;
                break;
            }
            if (text[i] == '\n') ++line, col = 0;
            i += len - 1;
            col += 1;
        }
        char what[64];
        std::snprintf(what, sizeof what, "invalid UTF-8 (byte 0x%02X at column %zu)", static_cast<unsigned char>(text[bad]), col);
        p.fail(static_cast<int>(line), what);
        return false;
    }
    std::size_t at = 0;
    for (int line = 1; at < text.size(); ++line) {
        std::size_t nl = text.find('\n', at);
        if (nl == std::string_view::npos) nl = text.size();
        const std::string_view raw_line = text.substr(at, nl - at);
        at = nl + 1;
        if (raw_line.size() > kMaxStringsLine) {
            p.fail(line, "a line longer than 4096 bytes");
            continue;
        }
        std::string_view s = trim(raw_line);
        if (s.empty() || s.front() == '#') continue;
        if (s.front() == '[') {
            const std::size_t close = s.find(']');
            const std::string_view rest = close == std::string_view::npos ? "" : trim(s.substr(close + 1));
            p.table = close == std::string_view::npos ? "" : std::string(s.substr(1, close - 1));
            if (close == std::string_view::npos || !dotted(p.table) || !(rest.empty() || rest.front() == '#'))
                p.fail(line, "a table header is [name] or [a.b]");
            continue;
        }
        const std::size_t eq = s.find('=');
        if (eq == std::string_view::npos) {
            p.fail(line, "expected key = \"text\"");
            continue;
        }
        const std::string key(trim(s.substr(0, eq)));
        std::string_view value = trim(s.substr(eq + 1));
        if (!dotted(key)) {
            p.fail(line, "a key is a dotted name of letters, digits, '_' and '-' (at most 128 bytes)");
            continue;
        }
        std::string decoded;
        bool is_string = false;
        const int key_line = line;
        if (value.substr(0, 3) == "\"\"\"") { // multi-line basic string
            std::string body(value.substr(3));
            bool closed = false;
            for (;;) {
                const std::size_t end = body.find("\"\"\"");
                if (end != std::string::npos) {
                    const std::string_view after = trim(std::string_view(body).substr(end + 3));
                    if (!after.empty() && after.front() != '#') p.fail(line, "text after a string");
                    body.resize(end);
                    closed = true;
                    break;
                }
                if (at >= text.size()) break;
                std::size_t next = text.find('\n', at);
                if (next == std::string_view::npos) next = text.size();
                std::string_view more = text.substr(at, next - at);
                at = next + 1;
                ++line;
                if (!more.empty() && more.back() == '\r') more.remove_suffix(1);
                if (!body.empty() && body.back() == '\\') { // a line-ending backslash joins the next line, trimmed
                    body.pop_back();
                    while (!more.empty() && (more.front() == ' ' || more.front() == '\t')) more.remove_prefix(1);
                } else if (!body.empty() || key_line != line - 1) {
                    body += '\n';
                }
                body += more;
            }
            if (!closed) {
                p.fail(key_line, "a multi-line string isn't closed (\"\"\")");
                continue;
            }
            const std::string why = unescape(body, decoded);
            if (!why.empty()) p.fail(key_line, why);
            is_string = true;
        } else if (!value.empty() && value.front() == '"') {
            std::size_t end = 1;
            while (end < value.size() && value[end] != '"') end += value[end] == '\\' ? 2 : 1;
            if (end >= value.size()) {
                p.fail(line, "a string isn't closed");
                continue;
            }
            const std::string_view after = trim(value.substr(end + 1));
            if (!after.empty() && after.front() != '#') p.fail(line, "text after a string");
            const std::string why = unescape(value.substr(1, end - 1), decoded);
            if (!why.empty()) p.fail(line, why);
            is_string = true;
        } else {
            const std::size_t hash = value.find('#');
            value = trim(value.substr(0, hash));
        }
        if (decoded.size() > kMaxStringBytes) p.fail(key_line, "a string longer than 4096 bytes");
        if (p.table.rfind("language", 0) == 0 && (p.table.size() == 8 || p.table[8] == '.')) {
            p.set_meta(key, value, decoded, is_string, key_line);
        } else if (!is_string) {
            p.fail(key_line, "a value is a string (\"text\" or \"\"\"text\"\"\")");
        } else {
            p.set_key(p.table.empty() ? key : p.table + "." + key, decoded, key_line);
        }
    }
    into.files.push_back(file);
    return p.ok;
}

bool read_language_list(std::string_view text, LanguageList& out) {
    out = LanguageList{};
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t nl = text.find('\n', at);
        if (nl == std::string_view::npos) nl = text.size();
        std::string_view line = trim(text.substr(at, nl - at));
        at = nl + 1;
        if (line.empty()) continue;
        const std::size_t tab = line.find('\t');
        if (tab == std::string_view::npos) return false;
        std::string code(line.substr(0, tab));
        if (!code.empty() && code[0] == '*') {
            code.erase(0, 1);
            out.base = static_cast<int>(out.codes.size());
        }
        std::vector<std::string> files;
        std::string_view rest = line.substr(tab + 1);
        while (!rest.empty()) {
            const std::size_t semi = rest.find(';');
            files.emplace_back(rest.substr(0, semi));
            rest = semi == std::string_view::npos ? std::string_view{} : rest.substr(semi + 1);
        }
        out.codes.push_back(code);
        out.files.push_back(files);
    }
    return !out.codes.empty();
}

} // namespace nnh::assetc
