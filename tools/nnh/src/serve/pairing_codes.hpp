#pragma once
// The pairing code nnh serve shows (spec 011 FR-3): 6 digits, valid for 2 minutes; a new one is made when
// a console that isn't paired connects and none is valid. Pure (time and randomness passed in); unit tested
// in tools/nnh/tests/test_serve.cpp.

#include <cstdint>
#include <string>

namespace nnh::tool {

class PairingCodes {
public:
    static constexpr std::int64_t kValidMs = 2 * 60 * 1000;

    /// The code valid at `now_ms`, "" if none.
    std::string current(std::int64_t now_ms) const;
    /// Makes a new code from `random`, valid from `now_ms`. @return it.
    std::string renew(std::int64_t now_ms, std::uint32_t random);
    /// Forgets the code (after a successful pairing it can't be used again).
    void spend() { code_.clear(); }

private:
    std::string code_;
    std::int64_t expires_ms_ = 0;
};

} // namespace nnh::tool
