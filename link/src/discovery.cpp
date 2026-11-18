#include "nnh_link/discovery.hpp"

#include <cstring>

namespace nnh::link {

namespace {

constexpr char kAnswerPrefix[] = "NNHSERVE1 ";
constexpr std::size_t kPrefixSize = sizeof kAnswerPrefix - 1;

} // namespace

bool is_discovery_question(const char* data, std::size_t size) {
    return size == sizeof kDiscoveryQuestion - 1 && std::memcmp(data, kDiscoveryQuestion, size) == 0;
}

std::string discovery_answer(const Id& server_id, const std::string& name) {
    std::string clean = name.substr(0, kMaxServerName);
    for (char& c : clean) {
        if (c < 0x20 || c > 0x7E) c = '?';
    }
    if (clean.empty()) clean = "nnh";
    return kAnswerPrefix + to_hex(server_id.data(), server_id.size()) + " " + clean;
}

bool parse_discovery_answer(const char* data, std::size_t size, Id& server_id, std::string& name) {
    const std::size_t hex = 2 * server_id.size();
    if (size < kPrefixSize + hex + 2 || std::memcmp(data, kAnswerPrefix, kPrefixSize) != 0) return false;
    if (!from_hex(std::string(data + kPrefixSize, hex), server_id.data(), server_id.size())) return false;
    if (data[kPrefixSize + hex] != ' ') return false;
    name.assign(data + kPrefixSize + hex + 1, size - kPrefixSize - hex - 1);
    if (name.empty() || name.size() > kMaxServerName) return false;
    for (char c : name) {
        if (c < 0x20 || c > 0x7E) return false;
    }
    return true;
}

} // namespace nnh::link
