// Fake platform: an in-memory file system (contract nnh/platform/fs.hpp). Files survive nnh::shutdown()
// and init() until platform_fake::reset(). Test-only library, so std containers are fine here.

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "fake_internal.hpp"
#include "nnh/platform/fs.hpp"

namespace nnh {
namespace {

std::map<std::string, std::vector<unsigned char>> g_files;
int g_failing_writes = 0;

void count_call() { ++platform_fake::detail::mutable_state().fs_calls; }

bool take_failure() {
    if (g_failing_writes == 0) return false;
    --g_failing_writes;
    return true;
}

} // namespace

const char* platform::fs_root() { return "fake:/data/"; }

std::int64_t platform::fs_size(const char* path) {
    count_call();
    const auto it = g_files.find(path);
    return it == g_files.end() ? -1 : static_cast<std::int64_t>(it->second.size());
}

std::int64_t platform::fs_read(const char* path, std::uint64_t offset, void* buffer, std::size_t capacity) {
    count_call();
    const auto it = g_files.find(path);
    if (it == g_files.end()) return -1;
    const auto& data = it->second;
    if (offset > data.size()) return 0;
    const std::size_t n = data.size() - offset < capacity ? data.size() - offset : capacity;
    if (n > 0) std::memcpy(buffer, data.data() + offset, n);
    return static_cast<std::int64_t>(n);
}

bool platform::fs_write(const char* path, const FsChunk* chunks, int count) {
    count_call();
    std::vector<unsigned char> data;
    for (int i = 0; i < count; ++i) {
        const auto* bytes = static_cast<const unsigned char*>(chunks[i].data);
        data.insert(data.end(), bytes, bytes + chunks[i].size);
    }
    if (take_failure()) {
        data.resize(data.size() / 2); // a torn write: half the content reached the card
        g_files[path] = data;
        return false;
    }
    g_files[path] = std::move(data);
    return true;
}

bool platform::fs_append(const char* path, const void* data, std::size_t size) {
    count_call();
    auto& file = g_files[path];
    const auto* bytes = static_cast<const unsigned char*>(data);
    file.insert(file.end(), bytes, bytes + size);
    return true;
}

bool platform::fs_rename(const char* from, const char* to) {
    count_call();
    const auto it = g_files.find(from);
    if (it == g_files.end() || g_files.count(to) != 0) return false;
    g_files[to] = std::move(it->second);
    g_files.erase(from);
    return true;
}

bool platform::fs_remove(const char* path) {
    count_call();
    g_files.erase(path);
    return true;
}

void platform_fake::detail::fs_reset() {
    g_files.clear();
    g_failing_writes = 0;
}

void platform_fake::fail_next_writes(int count) { g_failing_writes = count; }

void platform_fake::corrupt_file(const char* path, std::size_t byte_index) {
    const auto it = g_files.find(path);
    if (it != g_files.end() && byte_index < it->second.size()) it->second[byte_index] ^= 0x5A;
}

} // namespace nnh
