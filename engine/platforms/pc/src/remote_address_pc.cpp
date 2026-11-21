#include "remote_address_pc.hpp"

namespace nnh::platform::pc {
namespace {

/// Reads 1..`max_digits` decimal digits at `p` into `value`. @return the character after them, or null.
const char* read_number(const char* p, int max_digits, unsigned long& value) {
    value = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > max_digits) return nullptr;
        value = value * 10 + static_cast<unsigned long>(*p - '0');
        ++p;
    }
    return digits > 0 ? p : nullptr;
}

} // namespace

bool parse_remote_address(const char* text, RemoteAddress& out) {
    if (text == nullptr) return false;
    RemoteAddress a;
    const char* p = text;
    for (int i = 0; i < 4; ++i) {
        unsigned long part = 0;
        p = read_number(p, 3, part);
        if (p == nullptr || part > 255) return false;
        a.ip[i] = static_cast<std::uint8_t>(part);
        if (*p++ != (i < 3 ? '.' : ':')) return false;
    }
    unsigned long port = 0;
    p = read_number(p, 5, port);
    if (p == nullptr || *p != '\0' || port == 0 || port > 65535 || a.ip[0] != 127) return false;
    a.port = static_cast<std::uint16_t>(port);
    out = a;
    return true;
}

} // namespace nnh::platform::pc
