// The `--tunes FILE` reader (see tunes_file.hpp).

#include "remote/tunes_file.hpp"

#include "live/live_names.hpp"

namespace nnh::tool::remote {
namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

bool bare_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

/// The part of a line before a `#` that isn't inside double quotes.
std::string_view strip_comment(std::string_view s) {
    bool quoted = false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '"') quoted = !quoted;
        if (s[i] == '#' && !quoted) return s.substr(0, i);
    }
    return s;
}

/// A quoted string "..." (no escapes) → its text; false when not one.
bool unquote(std::string_view s, std::string& out) {
    if (s.size() < 2 || s.front() != '"' || s.back() != '"') return false;
    out.assign(s.substr(1, s.size() - 2));
    return out.find('"') == std::string::npos;
}

std::string name_problem(const std::string& name) {
    const auto e = detail::live::check_name(name.data(), name.size());
    if (e == detail::live_name::Error::None) return "";
    return "name \"" + name + "\" " + detail::live::name_error_text(e);
}

} // namespace

TunesFile read_tunes(std::string_view text, const std::string& file) {
    TunesFile out;
    if (text.size() > kMaxTunesBytes) {
        out.errors.push_back(file + ": the file is larger than 1 MiB");
        return out;
    }
    std::string table;
    int line = 0;
    for (std::size_t start = 0; start < text.size() && out.errors.size() < 20;) {
        std::size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) nl = text.size();
        const std::string_view l = trim(strip_comment(text.substr(start, nl - start)));
        start = nl + 1;
        ++line;
        const std::string where = file + ":" + std::to_string(line) + ": ";
        if (l.empty()) continue;
        if (l.front() == '[') {
            if (l.back() != ']' || l.size() < 3) {
                out.errors.push_back(where + "expected \"[group]\"");
                continue;
            }
            table.assign(l.substr(1, l.size() - 2));
            for (char& c : table) c = c == '.' ? '/' : c;
            const std::string why = name_problem(table);
            if (!why.empty()) out.errors.push_back(where + why);
            continue;
        }
        const std::size_t eq = l.find('=');
        if (eq == std::string_view::npos) {
            out.errors.push_back(where + "expected \"name = value\"");
            continue;
        }
        const std::string_view key = trim(l.substr(0, eq)), raw = trim(l.substr(eq + 1));
        std::string name;
        if (!unquote(key, name)) {
            bool bare = !key.empty();
            for (char c : key) bare = bare && bare_char(c);
            if (!bare) {
                out.errors.push_back(where + "expected \"name = value\"");
                continue;
            }
            name = table.empty() ? std::string(key) : table + "/" + std::string(key);
        }
        std::string value;
        if (raw.empty()) {
            out.errors.push_back(where + "expected \"name = value\"");
            continue;
        }
        if (!unquote(raw, value)) value.assign(raw);
        const std::string why = name_problem(name);
        if (!why.empty()) {
            out.errors.push_back(where + why);
            continue;
        }
        bool twice = false;
        for (const TuneValue& v : out.values) {
            if (v.name != name) continue;
            out.errors.push_back(where + "\"" + name + "\" is set twice (first at line " + std::to_string(v.line) + ")");
            twice = true;
            break;
        }
        if (twice) continue;
        if (out.values.size() >= kMaxTunesValues) {
            out.errors.push_back(where + "more than 4096 values");
            break;
        }
        out.values.push_back({name, value, line});
    }
    return out;
}

std::vector<Step> tunes_steps(const TunesFile& tunes, const std::string& file) {
    std::vector<Step> steps;
    for (const TuneValue& v : tunes.values) {
        Step s;
        s.command = Command::Set;
        s.line = 1;
        s.text = v.name;
        s.value = v.value;
        s.origin = file + ":" + std::to_string(v.line);
        steps.push_back(std::move(s));
    }
    Step resume;
    resume.command = Command::Resume;
    resume.line = 1;
    resume.origin = file;
    steps.push_back(std::move(resume));
    return steps;
}

} // namespace nnh::tool::remote
