// Integers as a language writes them (loc_number.hpp).

#include "loc/loc_number.hpp"

namespace nnh::detail::loc {

int format_integer(long long value, const char* grouping, int grouping_min, char* out) {
    // the magnitude as unsigned: INT64_MIN's is 2^63, which a long long can't hold
    const bool negative = value < 0;
    unsigned long long n = negative ? 0ull - static_cast<unsigned long long>(value) : static_cast<unsigned long long>(value);
    char digits[20];
    int count = 0;
    while (n > 0xFFFFFFFFull) { // a constant divisor: a multiply, no __aeabi call (64-bit: several)
        digits[count++] = static_cast<char>('0' + n % 10u);
        n /= 10u;
    }
    auto small = static_cast<std::uint32_t>(n); // the rest in 32 bits: one multiply per digit on the ARM11 (T10.3)
    do {
        digits[count++] = static_cast<char>('0' + small % 10u);
        small /= 10u;
    } while (small != 0);
    int sep = 0;
    while (grouping != nullptr && sep < 4 && grouping[sep] != 0) ++sep;
    const bool group = sep > 0 && count >= grouping_min;
    int at = 0;
    if (negative) out[at++] = '-';
    for (int i = count - 1; i >= 0; --i) {
        out[at++] = digits[i];
        if (group && i > 0 && i % 3 == 0) {
            for (int k = 0; k < sep; ++k) out[at++] = grouping[k];
        }
    }
    return at;
}

} // namespace nnh::detail::loc
