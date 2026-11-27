#pragma once
// Received files on the data folder (spec 031 D53, FR-18, X, II): `.nnh_reload/<n>.bin`, `n` a per-session counter the
// game picks (never a received name). The receiver's sink: bytes go through a 16 KB receive buffer (tag `reload`,
// allocated at the first batch, kept to shutdown) and reach the file with one fs_append when it fills, when a file ends
// or at flush(). The batch block (tag `reload`) lists the open or staged batch's files until the next batch. The first
// batch of a session wipes leftovers of a crashed run; a shutdown hook wipes the folder (Debug/Development only).
// Tests: tests/reload/test_reload_files.cpp.

#include <cstddef>
#include <cstdint>

#include "reload/reload_receiver.hpp"
#include "reload/reload_wire.hpp"

namespace nnh::detail::reload {

inline constexpr const char* kReloadFolder = ".nnh_reload";
inline constexpr std::size_t kReceiveBufferBytes = 16 * 1024;
/// Missing names in a row that end the wipe of leftovers.
inline constexpr int kWipeGap = 16;

/// One file of a batch.
struct BatchFile {
    const char* path = nullptr;     ///< NUL-terminated, in the batch block
    std::uint32_t hash = 0;         ///< path_hash(path)
    std::uint32_t size = 0;         ///< bytes (0 for a remove)
    std::uint16_t file = 0;         ///< `.nnh_reload/<file>.bin`; 0 for a remove
    remote::ReloadOp op = remote::ReloadOp::Put;
};

/// "<kReloadFolder>/<n>.bin" into `out`.
void reload_file_name(std::uint16_t n, char out[32]);

class ReloadFiles {
public:
    /// The first batch of the session: allocates the receive buffer and wipes leftovers. False without memory.
    bool ready();
    /// A batch begins: allocates its block (`files` entries, path bytes bounded by `total`). False without memory.
    bool begin_batch(std::uint16_t files, std::uint32_t total);
    /// The receiver's sink over this object.
    ReceiverSink sink();
    /// Where the next payload bytes may be read in place (the receive buffer's free part; `room` its bytes): a chunk
    /// read there and handed to the receiver is appended without a copy. Null before ready().
    std::uint8_t* window(std::size_t& room);
    /// Writes what the receive buffer holds (the end of a frame's reading). False on a write failure (the caller
    /// refuses the batch).
    bool flush();

    int count() const { return count_; }
    const BatchFile* files() const { return files_; }
    /// The batch is applied: its files now belong to the overlay; the block is kept until the next batch (the paths
    /// of this frame's list point into it).
    void keep_batch();
    /// Removes every file of the batch and frees its block (refused, cancelled, disconnected).
    void drop_batch();
    /// Removes one received file.
    static void remove_file(std::uint16_t n);
    /// Frees the buffer and the block (the shutdown hook; the files are wiped by the caller).
    void release();
    /// Removes `.nnh_reload/1.bin` … until kWipeGap names in a row are missing.
    static void wipe();

    bool write_failed() const { return write_failed_; }
    std::uint16_t next_file() const { return next_file_; }

private:
    static bool sink_begin(void* c, std::uint16_t index, const char* path, remote::ReloadOp op, std::uint32_t size);
    static bool sink_append(void* c, const std::uint8_t* data, std::size_t size);
    static bool sink_finish(void* c, std::uint16_t index);
    static void sink_discard(void* c);
    void free_block();

    std::uint8_t* buffer_ = nullptr;
    std::size_t buffered_ = 0;
    void* block_ = nullptr;          // the batch block: entries, then path bytes
    BatchFile* files_ = nullptr;
    char* paths_ = nullptr;
    std::size_t path_room_ = 0, path_used_ = 0;
    int capacity_ = 0, count_ = 0;
    std::uint16_t next_file_ = 1;    // the next file number of the session
    bool wiped_ = false;
    bool write_failed_ = false;
};

} // namespace nnh::detail::reload
