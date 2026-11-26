// Plural rules as bytecode (plural.hpp).

#include "loc/plural.hpp"

namespace nnh::detail::loc {
namespace {

std::uint32_t u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint64_t operand(std::uint64_t n, int modulus) {
    switch (modulus) {
    case 1: return n % 10u;
    case 2: return n % 100u;
    case 3: return n % 1000u;
    case 4: return n % 1000000u;
    default: return n;
    }
}

} // namespace

Category plural_category(const std::uint8_t* rule, std::size_t bytes, std::uint64_t n) {
    if (rule == nullptr || bytes == 0) return Other;
    const std::uint8_t* p = rule;
    const int categories = *p++;
    for (int c = 0; c < categories; ++c) {
        const auto category = static_cast<Category>(*p++);
        const int branches = *p++;
        bool found = false;
        for (int b = 0; b < branches; ++b) {
            const int conditions = *p++;
            bool all = true;
            for (int k = 0; k < conditions; ++k) {
                const int op = *p++;
                const int ranges = *p++;
                const std::uint64_t v = operand(n, op >> 1);
                bool in = false;
                for (int r = 0; r < ranges; ++r, p += 8) in = in || (v >= u32(p) && v <= u32(p + 4));
                all = all && ((op & 1) != 0 ? !in : in);
            }
            found = found || all;
        }
        if (found) return category;
    }
    return Other;
}

bool plural_valid(const std::uint8_t* rule, std::size_t bytes) {
    if (bytes == 0) return true;
    if (rule == nullptr || bytes > kMaxRuleBytes) return false;
    std::size_t at = 0;
    const auto take = [&](std::size_t n) {
        const bool ok = at + n <= bytes;
        at += n;
        return ok;
    };
    if (!take(1)) return false;
    int last = -1;
    for (int c = 0; c < rule[0]; ++c) {
        if (!take(2) || rule[at - 2] >= Other || rule[at - 2] <= last) return false;
        last = rule[at - 2];
        const int branches = rule[at - 1];
        for (int b = 0; b < branches; ++b) {
            if (!take(1)) return false;
            const int conditions = rule[at - 1];
            for (int k = 0; k < conditions; ++k) {
                if (!take(2) || (rule[at - 2] >> 1) > 4 || (rule[at - 2] >> 4) != 0) return false;
                const int ranges = rule[at - 1];
                for (int r = 0; r < ranges; ++r) {
                    if (!take(8) || u32(rule + at - 8) > u32(rule + at - 4)) return false;
                }
            }
        }
    }
    return at == bytes;
}

} // namespace nnh::detail::loc
