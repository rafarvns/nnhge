// Integer text (ui_number.hpp).

#include "ui/ui_number.hpp"

namespace nnh::detail {
namespace {

void put(char* out, int capacity, int& n, const char* s) {
    if (s == nullptr) return;
    while (*s != 0 && n < capacity) out[n++] = *s++;
}

} // namespace

int ui_format_number(char* out, int capacity, int value, const char* prefix, const char* suffix) {
    int n = 0;
    put(out, capacity, n, prefix);
    char digits[12];
    int d = 0;
    unsigned magnitude = value < 0 ? 0u - static_cast<unsigned>(value) : static_cast<unsigned>(value);
    do {
        digits[d++] = static_cast<char>('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0);
    if (value < 0) digits[d++] = '-';
    while (d > 0 && n < capacity) out[n++] = digits[--d];
    put(out, capacity, n, suffix);
    return n;
}

} // namespace nnh::detail
