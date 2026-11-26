// Compiling CLDR plural rule text (plural_compile.hpp).

#include "plural_compile.hpp"

#include <cstddef>
#include <sstream>

#include "loc/plural.hpp"

namespace nnh::assetc {
namespace {

const char* const kCategories[] = {"zero", "one", "two", "few", "many", "other"};

struct Condition {
    int modulus = 0;
    bool negate = false;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
};

void put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

/// A decimal of at most 10 digits.
bool number(const std::string& s, std::uint64_t& out) {
    if (s.empty() || s.size() > 10) return false;
    out = 0;
    for (const char c : s) {
        if (c < '0' || c > '9') return false;
        out = out * 10 + static_cast<std::uint64_t>(c - '0');
    }
    return true;
}

/// One relation "op [% m] (=|!=) list": a condition (on n/i), or a constant (`known`) for v w f t c e (all 0).
std::string relation(std::istringstream& in, Condition& c, bool& constant, bool& value) {
    std::string operand, word;
    in >> operand;
    const bool integer = operand == "n" || operand == "i";
    const bool zero = operand == "v" || operand == "w" || operand == "f" || operand == "t" || operand == "c" || operand == "e";
    if (!integer && !zero) return "unknown operand \"" + operand + "\"";
    in >> word;
    if (word == "%") {
        std::uint64_t m = 0;
        in >> m;
        c.modulus = m == 10 ? 1 : (m == 100 ? 2 : (m == 1000 ? 3 : (m == 1000000 ? 4 : -1)));
        if (c.modulus < 0) return "a modulus is 10, 100, 1000 or 1000000";
        in >> word;
    }
    if (word != "=" && word != "!=") return "expected = or != after \"" + operand + "\"";
    c.negate = word == "!=";
    std::string list;
    in >> list;
    std::istringstream items(list);
    std::string item;
    while (std::getline(items, item, ',')) {
        const std::size_t dots = item.find("..");
        std::uint64_t lo = 0, hi = 0;
        if (!number(item.substr(0, dots), lo) || (dots != std::string::npos && !number(item.substr(dots + 2), hi)))
            return "a bad value \"" + item + "\"";
        if (dots == std::string::npos) hi = lo;
        if (hi < lo || hi > 0xFFFFFFFFull) return "a bad range \"" + item + "\"";
        c.ranges.emplace_back(static_cast<std::uint32_t>(lo), static_cast<std::uint32_t>(hi));
    }
    if (c.ranges.empty()) return "a relation without values";
    constant = zero;
    if (zero) { // the operand is 0
        bool in_list = false;
        for (const auto& r : c.ranges) in_list = in_list || (r.first == 0);
        value = c.negate ? !in_list : in_list;
    }
    return "";
}

} // namespace

int category_index(const std::string& name) {
    for (int i = 0; i < 6; ++i)
        if (name == kCategories[i]) return i;
    return -1;
}

const char* category_name(int index) { return index >= 0 && index < 6 ? kCategories[index] : "?"; }

std::string compile_plural(const std::vector<PluralSource>& rules, std::vector<std::uint8_t>& out) {
    out.clear();
    std::vector<std::uint8_t> body;
    int count = 0, last = -1;
    for (const PluralSource& r : rules) {
        const int category = category_index(r.category);
        if (category < 0 || category == 5) return "\"" + r.category + "\" isn't a plural category (zero, one, two, few, many)";
        if (category <= last) return "categories in the order zero, one, two, few, many";
        last = category;
        std::vector<std::vector<Condition>> branches;
        std::istringstream text(r.rule);
        std::vector<Condition> branch;
        bool branch_false = false;
        for (std::string word;;) {
            Condition c;
            bool constant = false, value = false;
            const std::string why = relation(text, c, constant, value);
            if (!why.empty()) return r.category + ": " + why;
            if (constant) branch_false = branch_false || !value;
            else branch.push_back(c);
            if (!(text >> word)) break;
            if (word == "or") {
                if (!branch_false) branches.push_back(branch);
                branch.clear();
                branch_false = false;
            } else if (word != "and") {
                return r.category + ": expected and/or, found \"" + word + "\"";
            }
        }
        if (!branch_false) branches.push_back(branch);
        if (branches.empty()) continue; // never true for integers (e.g. "v != 0")
        ++count;
        body.push_back(static_cast<std::uint8_t>(category));
        body.push_back(static_cast<std::uint8_t>(branches.size()));
        for (const auto& b : branches) {
            body.push_back(static_cast<std::uint8_t>(b.size()));
            for (const Condition& c : b) {
                body.push_back(static_cast<std::uint8_t>(c.modulus << 1 | (c.negate ? 1 : 0)));
                body.push_back(static_cast<std::uint8_t>(c.ranges.size()));
                for (const auto& range : c.ranges) {
                    put32(body, range.first);
                    put32(body, range.second);
                }
            }
        }
    }
    if (count == 0) return "";
    out.push_back(static_cast<std::uint8_t>(count));
    out.insert(out.end(), body.begin(), body.end());
    if (out.size() > nnh::detail::loc::kMaxRuleBytes) return "the rule compiles to more than 255 bytes";
    return nnh::detail::loc::plural_valid(out.data(), out.size()) ? "" : "the rule doesn't compile";
}

std::uint8_t required_categories(const std::vector<std::uint8_t>& bytecode) {
    std::uint8_t bits = 1u << nnh::detail::loc::Other;
    const auto add = [&](std::uint64_t n) {
        bits |= static_cast<std::uint8_t>(1u << nnh::detail::loc::plural_category(bytecode.data(), bytecode.size(), n));
    };
    for (std::uint64_t n = 0; n <= 1000999; ++n) add(n);
    for (std::uint64_t p = 10; p <= 1000000000000000000ull; p *= 10) add(p);
    return bits;
}

} // namespace nnh::assetc
