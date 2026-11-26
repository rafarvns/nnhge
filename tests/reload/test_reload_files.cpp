// Received files on the data folder (spec 031 T2.1, FR-18, X, II): names pass nnh::fs's rules; bytes reach the file
// through the receive buffer; an SD write failure refuses the batch and removes its files; the wipe; nothing outside
// `.nnh_reload/`; the buffer allocated once.

#include <cstdint>
#include <cstring>

#include "core/crc32.hpp"
#include "nnh/app.hpp"
#include "nnh/fs.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/fs.hpp"
#include "nnh/platform_fake.hpp"
#include "reload/reload_files.hpp"
#include "reload/reload_memory.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using namespace nnh::detail::reload;
using namespace nnh::detail::remote;

namespace {

void fresh() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
}

/// Sends a put of `content` at `path` through the receiver into `files`.
ReceiverEvent put(ReloadReceiver& r, ReloadFiles& files, std::uint16_t index, const char* path, const void* content,
                  std::size_t size) {
    const ReceiverSink sink = files.sink();
    const auto len = static_cast<std::uint8_t>(std::strlen(path));
    r.on_message(reload_file_message(index, ReloadOp::Put, len, static_cast<std::uint32_t>(size)), sink);
    r.on_payload(reinterpret_cast<const std::uint8_t*>(path), len, sink);
    const std::uint32_t crc = nnh::detail::crc32(content, size);
    std::uint8_t c[4];
    for (int i = 0; i < 4; ++i) c[i] = static_cast<std::uint8_t>(crc >> (8 * i));
    ReceiverEvent e = r.on_payload(c, 4, sink);
    std::size_t at = 0;
    while (at < size && e == ReceiverEvent::None) {
        const std::size_t n = size - at < kReloadMaxChunk ? size - at : kReloadMaxChunk;
        r.on_message(reload_chunk_message(index, static_cast<std::uint16_t>(n), static_cast<std::uint32_t>(at)), sink);
        e = r.on_payload(static_cast<const std::uint8_t*>(content) + at, n, sink);
        at += n;
    }
    return e;
}

} // namespace

NNH_TEST(reload_files_names_pass_the_fs_rules) {
    char name[32];
    reload_file_name(1, name);
    CHECK(std::strcmp(name, ".nnh_reload/1.bin") == 0);
    CHECK(nnh::fs::validate_path(name).ok());
    reload_file_name(65534, name);
    CHECK(std::strcmp(name, ".nnh_reload/65534.bin") == 0);
    CHECK(nnh::fs::validate_path(name).ok());
}

NNH_TEST(reload_files_a_batch_reaches_the_data_folder) {
    fresh();
    ReloadFiles files;
    ReloadReceiver r;
    CHECK(files.ready());
    static std::uint8_t big[40000];
    for (std::size_t i = 0; i < sizeof big; ++i) big[i] = static_cast<std::uint8_t>(i * 7);
    CHECK(r.on_message(reload_begin_message(1, 2, 9 + sizeof big + 7), files.sink()) == ReceiverEvent::None);
    CHECK(files.begin_batch(2, 9 + sizeof big + 7));
    CHECK(put(r, files, 0, "art/a.png", big, sizeof big) == ReceiverEvent::None);
    const std::uint8_t name[] = {'o', 'l', 'd', '.', 'p', 'n', 'g'};
    r.on_message(reload_file_message(1, ReloadOp::Remove, 7, 0), files.sink());
    CHECK(r.on_payload(name, 7, files.sink()) == ReceiverEvent::Staged);
    CHECK(files.count() == 2);
    CHECK(std::strcmp(files.files()[0].path, "art/a.png") == 0);
    CHECK(files.files()[0].file == 1);
    CHECK(files.files()[0].size == sizeof big);
    CHECK(files.files()[1].op == ReloadOp::Remove && files.files()[1].file == 0);
    static std::uint8_t back[sizeof big];
    CHECK(nnh::platform::fs_read(".nnh_reload/1.bin", 0, back, sizeof back) == static_cast<std::int64_t>(sizeof big));
    CHECK(std::memcmp(back, big, sizeof big) == 0);
    files.release();
    nnh::shutdown();
}

NNH_TEST(reload_files_bytes_are_appended_a_buffer_at_a_time) {
    fresh();
    ReloadFiles files;
    ReloadReceiver r;
    files.ready();
    static std::uint8_t data[3 * 16384];
    r.on_message(reload_begin_message(1, 1, 5 + sizeof data), files.sink());
    files.begin_batch(1, 5 + sizeof data);
    fake::clear_fs_log();
    CHECK(put(r, files, 0, "a.bin", data, sizeof data) == ReceiverEvent::Staged);
    // a remove of a leftover name, then three appends of 16 KB (one per full buffer; the last at the file's end)
    CHECK(std::strcmp(fake::fs_log(), "XAAA") == 0);
    files.release();
    nnh::shutdown();
}

NNH_TEST(reload_files_a_write_failure_refuses_and_removes_the_files) {
    fresh();
    ReloadFiles files;
    ReloadReceiver r;
    files.ready();
    static std::uint8_t data[20000];
    r.on_message(reload_begin_message(1, 2, 5 + 4 + 5 + sizeof data), files.sink());
    files.begin_batch(2, 5 + 4 + 5 + sizeof data);
    CHECK(put(r, files, 0, "a.bin", "abcd", 4) == ReceiverEvent::None);
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == 4);
    fake::fail_next_appends(1); // b's first 16 KB don't reach the card
    CHECK(put(r, files, 1, "b.bin", data, sizeof data) == ReceiverEvent::Refused);
    CHECK(r.refusal().reason == ReloadReason::Write);
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == -1);
    CHECK(files.count() == 0);
    files.release();
    nnh::shutdown();
}

NNH_TEST(reload_files_the_first_batch_wipes_leftovers) {
    fresh();
    const char junk = 'j';
    nnh::platform::FsChunk chunk{&junk, 1};
    for (const char* n : {".nnh_reload/1.bin", ".nnh_reload/2.bin", ".nnh_reload/17.bin", ".nnh_reload/40.bin"}) {
        nnh::platform::fs_write(n, &chunk, 1);
    }
    nnh::platform::fs_write("save.dat", &chunk, 1);
    ReloadFiles files;
    CHECK(files.ready());
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == -1);
    CHECK(nnh::platform::fs_size(".nnh_reload/2.bin") == -1);
    CHECK(nnh::platform::fs_size(".nnh_reload/17.bin") == -1); // within the gap of 16 after 2
    CHECK(nnh::platform::fs_size(".nnh_reload/40.bin") == 1);  // past it: left (never read: the overlay doesn't name it)
    CHECK(nnh::platform::fs_size("save.dat") == 1);            // nothing outside the folder
    nnh::platform::fs_write(".nnh_reload/3.bin", &chunk, 1);
    CHECK(files.ready()); // only once per session
    CHECK(nnh::platform::fs_size(".nnh_reload/3.bin") == 1);
    files.release();
    nnh::shutdown();
}

NNH_TEST(reload_files_the_buffer_is_allocated_once) {
    fresh();
    ReloadFiles files;
    const std::size_t before = nnh::mem::tag_stats(reload_memory_tag()).bytes;
    CHECK(files.ready());
    CHECK(nnh::mem::tag_stats(reload_memory_tag()).bytes == before + kReceiveBufferBytes);
    CHECK(files.ready());
    CHECK(nnh::mem::tag_stats(reload_memory_tag()).bytes == before + kReceiveBufferBytes);
    files.release();
    CHECK(nnh::mem::tag_stats(reload_memory_tag()).bytes == before);
    nnh::shutdown();
}

NNH_TEST(reload_files_drop_removes_every_file_of_the_batch) {
    fresh();
    ReloadFiles files;
    ReloadReceiver r;
    files.ready();
    r.on_message(reload_begin_message(1, 3, 100), files.sink());
    files.begin_batch(3, 100);
    put(r, files, 0, "a.bin", "1", 1);
    put(r, files, 1, "b.bin", "22", 2);
    CHECK(r.on_message(reload_cancel_message(1), files.sink()) == ReceiverEvent::Cancelled);
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == -1);
    CHECK(nnh::platform::fs_size(".nnh_reload/2.bin") == -1);
    CHECK(files.count() == 0);
    CHECK(files.next_file() == 3); // names are never reused in a session
    files.release();
    nnh::shutdown();
}
