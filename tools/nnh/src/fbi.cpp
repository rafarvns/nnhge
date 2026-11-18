#include "fbi.hpp"

namespace nnh::tool {

std::vector<std::uint8_t> fbi_url_message(const std::vector<std::string>& urls) {
    std::string body;
    for (std::size_t i = 0; i < urls.size(); ++i) {
        if (i > 0) body += '\n';
        body += urls[i];
    }
    const auto n = static_cast<std::uint32_t>(body.size());
    std::vector<std::uint8_t> out = {static_cast<std::uint8_t>(n >> 24), static_cast<std::uint8_t>(n >> 16),
                                     static_cast<std::uint8_t>(n >> 8), static_cast<std::uint8_t>(n)};
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

std::string http_url(const std::string& host, std::uint16_t port, const std::string& name) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string url = "http://" + host + ":" + std::to_string(port) + "/";
    for (unsigned char c : name) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                           c == '.' || c == '_' || c == '~';
        if (plain) {
            url += static_cast<char>(c);
        } else {
            url += '%';
            url += kHex[c >> 4];
            url += kHex[c & 15];
        }
    }
    return url;
}

} // namespace nnh::tool
