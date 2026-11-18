#pragma once
// NNH Link's frames (spec 011, decision D24): `u32 length | u8 type | payload` on the wire. After the
// handshake every frame is sealed: payload = `u32 sequence | message | MAC-16`, MAC = the first 16 bytes
// of HMAC-SHA-256(session key, direction ‖ sequence ‖ type ‖ message). The direction byte stops a peer's
// own messages from being reflected back to it; sequences must go up by exactly one, so nothing can be
// replayed, dropped or reordered. Pure; unit tested in link/tests/test_messages.cpp.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "nnh_link/messages.hpp"
#include "nnh_link/sha256.hpp"

namespace nnh::link {

/// The largest frame accepted: a file chunk plus the sealing.
inline constexpr std::size_t kMaxFrame = kMaxFileChunk + 1024;

struct Frame {
    Type type = Type::Hello;
    std::vector<std::uint8_t> payload;
};

/// A plain frame (before the handshake ends).
std::vector<std::uint8_t> encode_frame(Type type, const std::vector<std::uint8_t>& payload);

/// Splits a byte stream into frames: feed() what arrives, then call next() until it returns nothing.
class FrameReader {
public:
    void feed(const void* data, std::size_t size);
    /// The next complete frame. Sets `error` (and stays broken) on a frame that is too long, empty or of an
    /// unknown type.
    std::optional<Frame> next(bool& error);

private:
    std::vector<std::uint8_t> buffer_;
    bool broken_ = false;
};

enum class Side : std::uint8_t { Console = 'C', Server = 'S' };

/// Seals what one side sends and opens what it receives, with the session key.
class SecureChannel {
public:
    SecureChannel(const Digest& session_key, Side me) : key_(session_key), me_(me) {}
    std::vector<std::uint8_t> seal(Type type, const std::vector<std::uint8_t>& message);
    /// The message inside a sealed frame from the other side; nullopt if the MAC or the sequence is wrong.
    std::optional<std::vector<std::uint8_t>> open(const Frame& frame);

private:
    std::array<std::uint8_t, 16> mac(Side from, std::uint32_t sequence, Type type, const std::uint8_t* message,
                                     std::size_t size) const;
    Digest key_;
    Side me_;
    std::uint32_t sent_ = 0;
    std::uint32_t received_ = 0;
};

} // namespace nnh::link
