// The overlay table (see reload_overlay.hpp).

#include "reload/reload_overlay.hpp"

#include <cstring>

namespace nnh::detail::reload {

std::uint32_t path_hash(const char* path) {
    std::uint32_t h = 0x811C9DC5u;
    for (const char* p = path; *p != '\0'; ++p) {
        h ^= static_cast<std::uint8_t>(*p);
        h *= 0x01000193u;
    }
    return h;
}

void OverlayTable::attach(OverlayEntry* slots) {
    slots_ = slots;
    paths_ = 0;
    for (int i = 0; i < kOverlaySlots; ++i) slots_[i] = OverlayEntry{};
}

OverlayEntry* OverlayTable::live_slot(const char* path, std::uint32_t hash) const {
    if (slots_ == nullptr) return nullptr;
    for (int i = 0; i < kOverlaySlots; ++i) {
        OverlayEntry& e = slots_[(hash + static_cast<std::uint32_t>(i)) % kOverlaySlots];
        if (e.used == 0) return nullptr; // never taken: the probe ends
        if (e.path != nullptr && e.hash == hash && std::strcmp(e.path, path) == 0) return &e;
    }
    return nullptr;
}

const OverlayEntry* OverlayTable::find(const char* path, std::uint32_t hash) const { return live_slot(path, hash); }

bool OverlayTable::put(const char* path, std::uint32_t hash, std::uint16_t file, std::uint32_t size,
                       std::uint16_t& replaced) {
    replaced = 0;
    if (OverlayEntry* live = live_slot(path, hash)) {
        replaced = live->file;
        live->file = file;
        live->size = size;
        live->path = path;
        return true;
    }
    if (slots_ == nullptr || paths_ >= kOverlayPaths) return false;
    for (int i = 0; i < kOverlaySlots; ++i) {
        OverlayEntry& e = slots_[(hash + static_cast<std::uint32_t>(i)) % kOverlaySlots];
        if (e.used != 0) continue; // tombstones stay: the probes of other paths go past them
        e = OverlayEntry{.hash = hash, .file = file, .used = 1, .size = size, .path = path};
        ++paths_;
        return true;
    }
    return false;
}

std::uint16_t OverlayTable::remove(const char* path, std::uint32_t hash) {
    OverlayEntry* live = live_slot(path, hash);
    if (live == nullptr) return 0;
    const std::uint16_t file = live->file;
    live->file = 0;
    live->size = 0;
    live->path = nullptr;
    return file;
}

void OverlayTable::repoint(const char* (*resolve)(void* context, const char* old), void* context,
                           void (*removed)(void* context, std::uint16_t file)) {
    if (slots_ == nullptr) return;
    for (int i = 0; i < kOverlaySlots; ++i) {
        OverlayEntry& e = slots_[i];
        if (e.path == nullptr) continue;
        const char* now = resolve(context, e.path);
        if (now != nullptr) {
            e.path = now;
            continue;
        }
        removed(context, e.file);
        e.file = 0;
        e.size = 0;
        e.path = nullptr;
    }
}

void OverlayTable::each_file(void (*fn)(void* context, std::uint16_t file), void* context) const {
    if (slots_ == nullptr) return;
    for (int i = 0; i < kOverlaySlots; ++i) {
        if (slots_[i].path != nullptr) fn(context, slots_[i].file);
    }
}

} // namespace nnh::detail::reload
