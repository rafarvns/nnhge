#include "address.hpp"

namespace nnh::tool {

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

} // namespace nnh::tool
