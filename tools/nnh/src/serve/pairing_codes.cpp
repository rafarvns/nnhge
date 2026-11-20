#include "serve/pairing_codes.hpp"

#include "nnh_link/handshake.hpp"

namespace nnh::tool {

std::string PairingCodes::current(std::int64_t now_ms) const {
    return now_ms < expires_ms_ ? code_ : std::string();
}

std::string PairingCodes::renew(std::int64_t now_ms, std::uint32_t random) {
    code_ = link::pairing_code(random);
    expires_ms_ = now_ms + kValidMs;
    return code_;
}

} // namespace nnh::tool
