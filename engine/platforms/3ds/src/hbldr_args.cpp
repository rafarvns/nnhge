// Arguments for Luma3DS's homebrew loader (see hbldr_args.hpp). Pure: tested on PC in nnh_tests.

#include "hbldr_args.hpp"

#include <cstring>

namespace nnh::platform::ctr {
namespace {

constexpr char kSdPrefix[] = "sdmc:";
constexpr char kExtension[] = ".3dsx";
constexpr char kReturnFlag[] = "--nnh-return=";

bool ends_with(const char* text, std::size_t length, const char* suffix) {
    const std::size_t n = std::strlen(suffix);
    return length >= n && std::memcmp(text + length - n, suffix, n) == 0;
}

/// Every '/'-separated segment after the first '/' is non-empty and not "..".
bool segments_ok(const char* path) {
    const char* segment = path + 1;
    for (;;) {
        const char* slash = std::strchr(segment, '/');
        const std::size_t length = slash != nullptr ? static_cast<std::size_t>(slash - segment) : std::strlen(segment);
        if (length == 0 || (length == 2 && segment[0] == '.' && segment[1] == '.')) return false;
        if (slash == nullptr) return true;
        segment = slash + 1;
    }
}

} // namespace

bool build_hbldr_argv(const char* path, const char* const* args, int count, HbldrArgv& out) {
    if (path == nullptr || count < 0 || (count > 0 && args == nullptr)) return false;
    std::memset(&out, 0, sizeof out);
    char* const begin = reinterpret_cast<char*>(out.words) + sizeof out.words[0];
    char* const end = reinterpret_cast<char*>(out.words) + sizeof out.words;
    char* dst = begin;
    for (int i = -1; i < count; ++i) {
        const char* arg = i < 0 ? path : args[i];
        if (arg == nullptr) return false;
        const std::size_t size = std::strlen(arg) + 1;
        if (size > static_cast<std::size_t>(end - dst)) return false;
        std::memcpy(dst, arg, size);
        dst += size;
    }
    out.words[0] = static_cast<std::uint32_t>(count + 1);
    return true;
}

const char* hbldr_target(const char* path) {
    if (path == nullptr || std::strncmp(path, kSdPrefix, sizeof kSdPrefix - 1) != 0) return nullptr;
    const char* target = path + sizeof kSdPrefix - 1;
    const std::size_t length = std::strlen(target);
    if (target[0] != '/' || length > kHbldrMaxPath || !ends_with(target, length, kExtension)) return nullptr;
    if (!segments_ok(target)) return nullptr;
    const char* name = std::strrchr(target, '/') + 1;
    if (std::strcmp(name, kExtension) == 0) return nullptr; // ".3dsx" alone
    return target;
}

const char* return_path(int argc, char* const* argv) {
    if (argv == nullptr) return nullptr;
    const char* found = nullptr;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (arg == nullptr || std::strncmp(arg, kReturnFlag, sizeof kReturnFlag - 1) != 0) continue;
        const char* path = arg + sizeof kReturnFlag - 1;
        found = hbldr_target(path) != nullptr ? path : nullptr;
    }
    return found;
}

} // namespace nnh::platform::ctr
