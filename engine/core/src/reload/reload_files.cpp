// Received files on the data folder (see reload_files.hpp).

#include "reload/reload_files.hpp"

#include <cstdio>
#include <cstring>

#include "nnh/memory.hpp"
#include "nnh/platform/fs.hpp"
#include "reload/reload_memory.hpp"
#include "reload/reload_overlay.hpp"

namespace nnh::detail::reload {

NNH_MEM_TAG(reload);

MemTag& reload_memory_tag() { return NNH_MEM_TAG_REF(reload); }

void reload_file_name(std::uint16_t n, char out[32]) {
    std::snprintf(out, 32, "%s/%u.bin", kReloadFolder, static_cast<unsigned>(n));
}

bool ReloadFiles::ready() {
    if (buffer_ == nullptr) {
        buffer_ = static_cast<std::uint8_t*>(mem::alloc(MemRegion::Heap, reload_memory_tag(), kReceiveBufferBytes));
        if (buffer_ == nullptr) return false;
    }
    if (!wiped_) {
        wipe();
        wiped_ = true;
    }
    return true;
}

void ReloadFiles::free_block() {
    mem::free(block_);
    block_ = nullptr;
    files_ = nullptr;
    paths_ = nullptr;
    capacity_ = count_ = 0;
    path_room_ = path_used_ = 0;
}

bool ReloadFiles::begin_batch(std::uint16_t files, std::uint32_t total) {
    free_block();
    buffered_ = 0;
    write_failed_ = false;
    // Path bytes: at most 255 per file and never more than the batch's total, plus a NUL each.
    const std::size_t paths = (total < files * 255u ? total : files * 255u) + files;
    const std::size_t bytes = sizeof(BatchFile) * files + paths;
    block_ = mem::alloc(MemRegion::Heap, reload_memory_tag(), bytes);
    if (block_ == nullptr) return false;
    files_ = static_cast<BatchFile*>(block_);
    paths_ = reinterpret_cast<char*>(files_ + files);
    capacity_ = files;
    path_room_ = paths;
    return true;
}

std::uint8_t* ReloadFiles::window(std::size_t& room) {
    room = buffer_ != nullptr ? kReceiveBufferBytes - buffered_ : 0;
    return buffer_ != nullptr ? buffer_ + buffered_ : nullptr;
}

ReceiverSink ReloadFiles::sink() { return ReceiverSink{this, sink_begin, sink_append, sink_finish, sink_discard}; }

bool ReloadFiles::flush() {
    if (buffered_ == 0 || count_ == 0) return !write_failed_;
    char name[32];
    reload_file_name(files_[count_ - 1].file, name);
    if (!platform::fs_append(name, buffer_, buffered_)) write_failed_ = true;
    buffered_ = 0;
    return !write_failed_;
}

bool ReloadFiles::sink_begin(void* c, std::uint16_t, const char* path, remote::ReloadOp op, std::uint32_t size) {
    auto* self = static_cast<ReloadFiles*>(c);
    const std::size_t length = std::strlen(path);
    if (self->count_ >= self->capacity_ || self->path_used_ + length + 1 > self->path_room_) return false;
    if (op == remote::ReloadOp::Put && self->next_file_ == 0xFFFF) return false; // a session of 65534 files: restart
    char* stored = self->paths_ + self->path_used_;
    std::memcpy(stored, path, length + 1);
    self->path_used_ += length + 1;
    BatchFile& f = self->files_[self->count_++];
    f = BatchFile{.path = stored, .hash = path_hash(stored), .size = size, .file = 0, .op = op};
    if (op == remote::ReloadOp::Remove) return true;
    f.file = self->next_file_++;
    char name[32];
    reload_file_name(f.file, name);
    // An empty file to append to: replaces a leftover of the same name (a crashed run past the wipe's gap) and makes
    // the folder the first time (fs_write creates parent folders; fs_append doesn't).
    return platform::fs_write(name, nullptr, 0);
}

bool ReloadFiles::sink_append(void* c, const std::uint8_t* data, std::size_t size) {
    auto* self = static_cast<ReloadFiles*>(c);
    if (data == self->buffer_ + self->buffered_ && size <= kReceiveBufferBytes - self->buffered_) { // read in place
        self->buffered_ += size;
        if (self->buffered_ == kReceiveBufferBytes && !self->flush()) return false;
        return !self->write_failed_;
    }
    while (size > 0) {
        const std::size_t room = kReceiveBufferBytes - self->buffered_;
        const std::size_t n = size < room ? size : room;
        std::memcpy(self->buffer_ + self->buffered_, data, n);
        self->buffered_ += n;
        data += n;
        size -= n;
        if (self->buffered_ == kReceiveBufferBytes && !self->flush()) return false;
    }
    return !self->write_failed_;
}

bool ReloadFiles::sink_finish(void* c, std::uint16_t) { return static_cast<ReloadFiles*>(c)->flush(); }

void ReloadFiles::sink_discard(void* c) { static_cast<ReloadFiles*>(c)->drop_batch(); }

void ReloadFiles::keep_batch() { buffered_ = 0; }

void ReloadFiles::drop_batch() {
    for (int i = 0; i < count_; ++i) {
        if (files_[i].file != 0) remove_file(files_[i].file);
    }
    buffered_ = 0;
    free_block();
}

void ReloadFiles::remove_file(std::uint16_t n) {
    char name[32];
    reload_file_name(n, name);
    platform::fs_remove(name);
}

void ReloadFiles::release() {
    free_block();
    mem::free(buffer_);
    buffer_ = nullptr;
    buffered_ = 0;
    wiped_ = false;
    next_file_ = 1;
}

void ReloadFiles::wipe() {
    char name[32];
    int missing = 0;
    for (std::uint32_t n = 1; n < 0xFFFF && missing < kWipeGap; ++n) {
        reload_file_name(static_cast<std::uint16_t>(n), name);
        if (platform::fs_size(name) < 0) {
            ++missing;
            continue;
        }
        missing = 0;
        platform::fs_remove(name);
    }
}

} // namespace nnh::detail::reload
