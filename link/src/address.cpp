#include "nnh_link/address.hpp"

namespace nnh::link {

std::optional<Ipv4> parse_ipv4(const std::string& text) {
    Ipv4 out;
    std::size_t at = 0;
    for (int part = 0; part < 4; ++part) {
        if (part > 0) {
            if (at >= text.size() || text[at] != '.') return std::nullopt;
            ++at;
        }
        const std::size_t start = at;
        unsigned value = 0;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9' && at - start < 3) {
            value = value * 10 + static_cast<unsigned>(text[at] - '0');
            ++at;
        }
        const std::size_t digits = at - start;
        if (digits == 0 || value > 255 || (digits > 1 && text[start] == '0')) return std::nullopt;
        out.b[part] = static_cast<std::uint8_t>(value);
    }
    if (at != text.size()) return std::nullopt;
    return out;
}

std::string to_string(Ipv4 a) {
    return std::to_string(a.b[0]) + "." + std::to_string(a.b[1]) + "." + std::to_string(a.b[2]) + "." +
           std::to_string(a.b[3]);
}

bool is_local_network(Ipv4 a) {
    return a.b[0] == 10 || a.b[0] == 127 || (a.b[0] == 172 && a.b[1] >= 16 && a.b[1] <= 31) ||
           (a.b[0] == 192 && a.b[1] == 168) || (a.b[0] == 169 && a.b[1] == 254);
}

Ipv4 broadcast_of(Ipv4 address, int prefix_length) {
    const std::uint32_t value = std::uint32_t(address.b[0]) << 24 | std::uint32_t(address.b[1]) << 16 |
                                std::uint32_t(address.b[2]) << 8 | address.b[3];
    const std::uint32_t host_bits = prefix_length <= 0 ? 0xFFFFFFFFu : (prefix_length >= 32 ? 0u : 0xFFFFFFFFu >> prefix_length);
    const std::uint32_t b = value | host_bits;
    return Ipv4{{std::uint8_t(b >> 24), std::uint8_t(b >> 16), std::uint8_t(b >> 8), std::uint8_t(b)}};
}

} // namespace nnh::link
