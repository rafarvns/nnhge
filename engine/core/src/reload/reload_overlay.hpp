#pragma once
// The overlay table (spec 031 D53, FR-5, FR-12, FR-16): asset path → the received file that replaces it
// (`.nnh_reload/<n>.bin`). Open addressing over kOverlaySlots entries in a caller-given array (allocated at the first
// batch, tag `reload`); at most kOverlayPaths live paths (more: the batch is refused, "restart"). Removing a path leaves a
// tombstone that later puts reuse, so a session may add and remove paths any number of times. Entries point at their
// path in the current asset index (repoint() when a batch swaps the index), so lookups compare exactly; the hash only
// picks the slot.
// Pure, no heap. Tests: tests/reload/test_reload_overlay.cpp.

#include <cstddef>
#include <cstdint>

namespace nnh::detail::reload {

inline constexpr int kOverlaySlots = 512;
inline constexpr int kOverlayPaths = 256;

/// FNV-1a 32 of a NUL-terminated path (the overlay's slot hash, and Q12's per-object path hash).
std::uint32_t path_hash(const char* path);

struct OverlayEntry {
    std::uint32_t hash = 0;
    std::uint16_t file = 0;      ///< `.nnh_reload/<file>.bin`; 0 while removed
    std::uint16_t used = 0;      ///< 1 once a path took the slot
    std::uint32_t size = 0;      ///< the file's bytes
    const char* path = nullptr;  ///< in the current asset index (null while removed)
};

class OverlayTable {
public:
    /// Uses `slots` (kOverlaySlots entries, zeroed by the table). Forgets every entry.
    void attach(OverlayEntry* slots);
    bool attached() const { return slots_ != nullptr; }

    /// The live entry for `path` (exact compare), or nullptr (not in the overlay, or removed).
    const OverlayEntry* find(const char* path, std::uint32_t hash) const;
    /// Live paths (received files the overlay names).
    int paths() const { return paths_; }

    /// `path` (stable: in the current index) now reads from `file` of `size` bytes; `replaced` receives the file it
    /// replaces (0: none), to remove. False when the path isn't live and kOverlayPaths paths are (check find() and
    /// paths() first).
    bool put(const char* path, std::uint32_t hash, std::uint16_t file, std::uint32_t size, std::uint16_t& replaced);
    /// `path` no longer reads from a received file. @return the file to remove (0: none).
    std::uint16_t remove(const char* path, std::uint32_t hash);

    /// Points every live entry at the path `resolve(context, old)` returns (the same text in a new index); an entry
    /// whose path the resolver doesn't find is marked removed (its file returned through `removed`, once per entry).
    void repoint(const char* (*resolve)(void* context, const char* old), void* context,
                 void (*removed)(void* context, std::uint16_t file));

    /// Calls `fn(context, file)` for every live file (the wipe at shutdown, a detach).
    void each_file(void (*fn)(void* context, std::uint16_t file), void* context) const;

private:
    OverlayEntry* live_slot(const char* path, std::uint32_t hash) const;
    OverlayEntry* slots_ = nullptr;
    int paths_ = 0;
};

} // namespace nnh::detail::reload
