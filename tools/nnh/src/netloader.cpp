#include "netloader.hpp"

#include <cstring>

namespace nnh::tool {

namespace {

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

} // namespace

bool valid_netloader_name(const std::string& name) {
    static constexpr char kExt[] = ".3dsx";
    constexpr std::size_t kExtLen = sizeof kExt - 1;
    if (name.size() <= kExtLen || name.size() > 200 || name.compare(name.size() - kExtLen, kExtLen, kExt) != 0) {
        return false;
    }
    for (char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                        c == '_' || c == '-';
        if (!ok) return false;
    }
    return name.find("..") == std::string::npos;
}

std::vector<std::uint8_t> netloader_header(const std::string& name, std::uint32_t file_size) {
    std::vector<std::uint8_t> out;
    put_u32(out, static_cast<std::uint32_t>(name.size()));
    out.insert(out.end(), name.begin(), name.end());
    put_u32(out, file_size);
    return out;
}

std::vector<std::vector<std::uint8_t>> netloader_chunks(const std::vector<std::uint8_t>& stream) {
    std::vector<std::vector<std::uint8_t>> chunks;
    for (std::size_t at = 0; at < stream.size(); at += kMaxNetloaderChunk) {
        const std::size_t n = stream.size() - at < kMaxNetloaderChunk ? stream.size() - at : kMaxNetloaderChunk;
        std::vector<std::uint8_t> chunk;
        chunk.reserve(4 + n);
        put_u32(chunk, static_cast<std::uint32_t>(n));
        chunk.insert(chunk.end(), stream.begin() + static_cast<long>(at), stream.begin() + static_cast<long>(at + n));
        chunks.push_back(std::move(chunk));
    }
    return chunks;
}

std::vector<std::uint8_t> netloader_args(const std::vector<std::string>& args) {
    std::vector<std::uint8_t> body;
    for (const std::string& a : args) {
        body.insert(body.end(), a.begin(), a.end());
        body.push_back(0);
    }
    std::vector<std::uint8_t> out;
    put_u32(out, static_cast<std::uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

std::string netloader_path(const std::string& name) { return "sdmc:/3ds/" + name; }

bool is_discovery_reply(const char* data, std::size_t size) {
    constexpr std::size_t n = sizeof kDiscoveryReply - 1;
    return size >= n && std::memcmp(data, kDiscoveryReply, n) == 0;
}

} // namespace nnh::tool
