// NNH_SAVE_DUMP (spec 027, FR-17, plan decision 15): in Debug and Development builds, when the environment
// variable was set at the first store (read once with std::getenv; the 3DS has no environment), every store
// also writes `<path>.txt`, the dump of what it stored, through fs_write and fs_append in 4 KB pieces (not
// atomic: a debugging aid, outside FR-6's file operation count). Release compiles none of it.

#include "nnh/fs.hpp"
#include "nnh/platform/fs.hpp"
#include "saves/save_internal.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nnh::detail {

#if !defined(NNH_FLAVOR_RELEASE)
namespace {

int g_override = -1; // the test hook: -1 environment, 0 off, 1 on
int g_from_env = -1; // the environment, read once

bool dump_enabled() {
    if (g_override >= 0) return g_override == 1;
    if (g_from_env < 0) {
        const char* value = std::getenv("NNH_SAVE_DUMP");
        g_from_env = value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0 ? 1 : 0;
    }
    return g_from_env == 1;
}

/// Lines gathered in a 4 KB static buffer, written to the file whenever it fills.
struct TextFile {
    const char* path = nullptr;
    bool started = false;
    bool failed = false;
    std::size_t used = 0;
    static inline char buffer[4096];

    void flush() {
        if (used == 0 || failed) return;
        if (!started) {
            const platform::FsChunk chunk{buffer, used};
            failed = !platform::fs_write(path, &chunk, 1);
            started = true;
        } else {
            failed = !platform::fs_append(path, buffer, used);
        }
        used = 0;
    }
    void add(const char* line) {
        const std::size_t n = std::strlen(line);
        if (used + n + 1 > sizeof buffer) flush();
        if (n + 1 > sizeof buffer) return;
        std::memcpy(buffer + used, line, n);
        used += n;
        buffer[used++] = '\n';
    }
};

void add_line(void* context, const char* line) {
    static_cast<TextFile*>(context)->add(line);
}

} // namespace

void save_set_dump_override(int mode) {
    g_override = mode;
}

void save_dump_on_store(const SaveTypeInfo& type, const char* path, const std::uint8_t* data,
                        std::size_t size) {
    if (!dump_enabled()) return;
    char text[fs::kMaxPathLength + 8];
    std::snprintf(text, sizeof text, "%s.txt", path);
    if (!fs::validate_path(text)) return;
    TextFile file;
    file.path = text;
    if (!save_dump_bytes(&type, data, size, add_line, &file)) return;
    file.flush();
}

#else

void save_set_dump_override(int) {}
void save_dump_on_store(const SaveTypeInfo&, const char*, const std::uint8_t*, std::size_t) {}

#endif

} // namespace nnh::detail
