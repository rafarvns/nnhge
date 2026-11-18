#pragma once
// NNH Link's messages (spec 011, plan "Protocol"): their types, payloads and limits. Each payload has an
// encode() and a decode() that rejects anything malformed, oversized or with bytes left over. Pure;
// unit tested and fuzzed (link/tests/test_messages.cpp, tests/fuzz).

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "nnh_link/sha256.hpp"

namespace nnh::link {

inline constexpr std::uint16_t kLinkPort = 17492;
inline constexpr std::uint16_t kProtocolVersion = 1;

enum class Type : std::uint8_t {
    Hello = 1,
    Pair = 2,
    Paired = 3,
    Refused = 4,
    Proof = 5,
    ListGames = 6,
    Games = 7,
    Build = 8,
    BuildLine = 9,
    BuildFailed = 10,
    BuildDone = 11,
    FileChunk = 12,
    Launching = 13,
};
bool valid_type(std::uint8_t t);

using Id = std::array<std::uint8_t, 16>;
using Nonce = std::array<std::uint8_t, 16>;

// Limits (bytes / counts).
inline constexpr std::size_t kMaxName = 64;      // game names, titles
inline constexpr std::size_t kMaxText = 256;     // a build line, a reason
inline constexpr std::size_t kMaxGames = 256;
inline constexpr std::size_t kMaxErrorLines = 20;
inline constexpr std::size_t kMaxFileChunk = 16 * 1024;

struct Hello {
    std::uint16_t version = kProtocolVersion;
    Id id{};
    Nonce nonce{};
};
struct GameInfo {
    std::string name;  ///< the CMake target (what Build asks for)
    std::string title; ///< what the console shows
};
struct BuildRequest {
    std::string game;
    bool release = false;
};
struct BuildResult {
    std::uint64_t size = 0; ///< bytes of the .3dsx that follows in FileChunks
    Digest sha256{};
};

std::vector<std::uint8_t> encode(const Hello& m);
bool decode(const std::vector<std::uint8_t>& p, Hello& m);
std::vector<std::uint8_t> encode_digest(const Digest& d); ///< Pair, Proof
bool decode_digest(const std::vector<std::uint8_t>& p, Digest& d);
std::vector<std::uint8_t> encode_text(const std::string& s); ///< Refused, BuildLine, Launching
bool decode_text(const std::vector<std::uint8_t>& p, std::string& s);
std::vector<std::uint8_t> encode(const std::vector<GameInfo>& games);
bool decode(const std::vector<std::uint8_t>& p, std::vector<GameInfo>& games);
std::vector<std::uint8_t> encode(const BuildRequest& m);
bool decode(const std::vector<std::uint8_t>& p, BuildRequest& m);
std::vector<std::uint8_t> encode_lines(const std::vector<std::string>& lines); ///< BuildFailed
bool decode_lines(const std::vector<std::uint8_t>& p, std::vector<std::string>& lines);
std::vector<std::uint8_t> encode(const BuildResult& m);
bool decode(const std::vector<std::uint8_t>& p, BuildResult& m);

/// A game name the protocol accepts: 1-64 of letters, digits, '_', '-'.
bool valid_game_name(const std::string& name);

} // namespace nnh::link
