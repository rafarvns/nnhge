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
// Power cuts (spec 027 AC-3): -1 = none scheduled; otherwise mutating calls left before the cut.
int g_power_ops = -1;
bool g_power_off = false;
bool g_power_torn = true;
std::string g_fs_log; // one letter per contract call (fs_log())

void count_call(char letter) {
    ++platform_fake::detail::mutable_state().fs_calls;
    if (g_fs_log.size() < 255) g_fs_log += letter;
}

enum class Power { On, Cut, Off };

/// A mutating call asks whether it runs (On), is the one the power cut stops (Cut), or comes after it (Off).
Power power_step() {
    if (g_power_off) return Power::Off;
    if (g_power_ops < 0) return Power::On;
    if (g_power_ops == 0) {
        g_power_off = true;
        return Power::Cut;
    }
    --g_power_ops;
    return Power::On;
}

bool take_failure() {
    if (g_failing_writes == 0) return false;
    --g_failing_writes;
    return true;
}

} // namespace

const char* platform::fs_root() { return "fake:/data/"; }

std::int64_t platform::fs_size(const char* path) {
    count_call('S');
    const auto it = g_files.find(path);
    return it == g_files.end() ? -1 : static_cast<std::int64_t>(it->second.size());
}

std::int64_t platform::fs_read(const char* path, std::uint64_t offset, void* buffer, std::size_t capacity) {
    count_call('R');
    const auto it = g_files.find(path);
    if (it == g_files.end()) return -1;
    const auto& data = it->second;
    if (offset > data.size()) return 0;
    const std::size_t n = data.size() - offset < capacity ? data.size() - offset : capacity;
    if (n > 0) std::memcpy(buffer, data.data() + offset, n);
    return static_cast<std::int64_t>(n);
}

bool platform::fs_write(const char* path, const FsChunk* chunks, int count) {
    count_call('W');
    const Power power = power_step();
    if (power == Power::Off) return false;
    std::vector<unsigned char> data;
    for (int i = 0; i < count; ++i) {
        const auto* bytes = static_cast<const unsigned char*>(chunks[i].data);
        data.insert(data.end(), bytes, bytes + chunks[i].size);
    }
    if (power == Power::Cut) {
        if (g_power_torn) {
            data.resize(data.size() / 2);
            g_files[path] = data;
        }
        return false;
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
    count_call('A');
    const Power power = power_step();
    if (power == Power::Off || (power == Power::Cut && !g_power_torn)) return false;
    auto& file = g_files[path];
    const auto* bytes = static_cast<const unsigned char*>(data);
    file.insert(file.end(), bytes, bytes + (power == Power::Cut ? size / 2 : size));
    return power == Power::On;
}

bool platform::fs_rename(const char* from, const char* to) {
    count_call('N');
    if (power_step() != Power::On) return false;
    const auto it = g_files.find(from);
    if (it == g_files.end() || g_files.count(to) != 0) return false;
    g_files[to] = std::move(it->second);
    g_files.erase(from);
    return true;
}

bool platform::fs_remove(const char* path) {
    count_call('X');
    if (power_step() != Power::On) return false;
    g_files.erase(path);
    return true;
}

void platform_fake::detail::fs_reset() {
    g_files.clear();
    g_failing_writes = 0;
    g_power_ops = -1;
    g_power_off = false;
    g_power_torn = true;
    g_fs_log.clear();
}

void platform_fake::fail_next_writes(int count) { g_failing_writes = count; }

void platform_fake::corrupt_file(const char* path, std::size_t byte_index) {
    const auto it = g_files.find(path);
    if (it != g_files.end() && byte_index < it->second.size()) it->second[byte_index] ^= 0x5A;
}

void platform_fake::cut_power_after(int ops, bool torn) {
    g_power_ops = ops < 0 ? 0 : ops;
    g_power_off = false;
    g_power_torn = torn;
}

void platform_fake::restore_power() {
    g_power_ops = -1;
    g_power_off = false;
}

void platform_fake::truncate_file(const char* path, std::size_t size) {
    const auto it = g_files.find(path);
    if (it != g_files.end() && size < it->second.size()) it->second.resize(size);
}

const char* platform_fake::fs_log() { return g_fs_log.c_str(); }

void platform_fake::clear_fs_log() { g_fs_log.clear(); }

} // namespace nnh
