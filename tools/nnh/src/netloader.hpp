#pragma once
// The Homebrew Launcher netloader's messages (what devkitPro's 3dslink sends; spec 010, decision D23).
// Facts from the launcher's netloader.c: TCP and UDP port 17491; integers little-endian; the file is saved
// as sdmc:/3ds/<name>; the data is a zlib stream sent in chunks of at most 16 384 bytes (the console's
// buffer size, which it does not check); argv[0] must be that path (libctru opens the romfs from it).
// Pure; unit tested in tools/nnh/tests/test_netloader.cpp.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nnh::tool {

inline constexpr std::uint16_t kNetloaderPort = 17491;
inline constexpr std::size_t kMaxNetloaderChunk = 16384;
inline constexpr char kDiscoveryRequest[] = "3dsboot";
inline constexpr char kDiscoveryReply[] = "boot3ds";

/// A file name the console accepts: letters, digits, '.', '_', '-', ending in ".3dsx", 1 to 200 chars.
bool valid_netloader_name(const std::string& name);

/// int32 name length, the name, uint32 file length.
std::vector<std::uint8_t> netloader_header(const std::string& name, std::uint32_t file_size);

/// The stream cut into wire chunks: uint32 size (at most kMaxNetloaderChunk) followed by the bytes.
std::vector<std::vector<std::uint8_t>> netloader_chunks(const std::vector<std::uint8_t>& stream);

/// int32 total length, then each argument followed by a NUL.
std::vector<std::uint8_t> netloader_args(const std::vector<std::string>& args);

/// Where the console saves `name`, which is also the game's argv[0].
std::string netloader_path(const std::string& name);

/// True if a UDP datagram is the console's discovery reply.
bool is_discovery_reply(const char* data, std::size_t size);

} // namespace nnh::tool
