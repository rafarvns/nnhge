// Fuzz target bodies (see fuzz_targets.hpp).

#include "fuzz_targets.hpp"

#include <cstdlib>
#include <cstring>

#include "assets/asset_index.hpp"
#include "audio/ogg_reader.hpp"
#include "audio/wav_reader.hpp"
#include "fs/save_format.hpp"
#include "nnh/fs.hpp"
#include "nnh/platform/input.hpp"
#include "remote/remote_input.hpp"
#include "remote/remote_reader.hpp"
#include "remote/remote_schedule.hpp"

namespace nnh::fuzz {

int path_target(const std::uint8_t* data, std::size_t size) {
    char path[512];
    const std::size_t n = size < sizeof path - 1 ? size : sizeof path - 1;
    std::memcpy(path, data, n);
    path[n] = '\0';
    (void)fs::validate_path(path);
    return 0;
}

int save_target(const std::uint8_t* data, std::size_t size) {
    const auto view = detail::validate_save(data, size);
    if (view) {
        // A valid view must point inside the input.
        const auto& v = view.value();
        if (v.payload < data || v.payload + v.size > data + size) std::abort();
    }
    return 0;
}

int asset_index_target(const std::uint8_t* data, std::size_t size) {
    // The index file comes from the disk: parsing must stay in bounds, and every entry it accepts must
    // lie inside the buffer and be found again by lookup.
    static char text[4097];
    static const char* entries[64];
    const std::size_t n = size < sizeof text - 1 ? size : sizeof text - 1;
    std::memcpy(text, data, n);
    detail::AssetIndex index;
    if (index.parse(text, n, entries, 64)) {
        for (int i = 0; i < index.count(); ++i) {
            if (entries[i] < text || entries[i] >= text + n + 1) std::abort();
            if (index.find(entries[i]) == nullptr) std::abort();
        }
    }
    return 0;
}

int wav_target(const std::uint8_t* data, std::size_t size) {
    // Whole file, and just its first half as a streaming header: an accepted format must describe
    // whole frames inside the file.
    for (std::size_t header : {size, size / 2}) {
        const auto wav = detail::parse_wav(data, header, size);
        if (!wav) continue;
        const auto& w = wav.value();
        const std::size_t frame = static_cast<std::size_t>(w.channels * w.bits / 8);
        if (frame == 0 || w.data_size == 0 || w.data_size % frame != 0) std::abort();
        if (w.data_offset > size || w.data_size > size - w.data_offset) std::abort();
    }
    return 0;
}

namespace {

struct FuzzBytes {
    const std::uint8_t* data;
    std::size_t size;
};

std::size_t read_fuzz_bytes(void* context, std::uint64_t offset, void* out, std::size_t n) {
    const auto* bytes = static_cast<const FuzzBytes*>(context);
    if (offset >= bytes->size) return 0;
    const std::size_t count = n < bytes->size - offset ? n : static_cast<std::size_t>(bytes->size - offset);
    std::memcpy(out, bytes->data + offset, count);
    return count;
}

/// Every 16-byte window that decodes must encode back to exactly the same bytes (nothing is lost or invented).
void check_remote_round_trips(const std::uint8_t* data, std::size_t size) {
    using namespace detail::remote;
    for (std::size_t i = 0; i + kMessageSize <= size; ++i) {
        Message m;
        if (decode_message(data + i, m) != Reject::None) continue;
        std::uint8_t again[kMessageSize];
        encode_message(m, again);
        if (std::memcmp(again, data + i, kMessageSize) != 0) std::abort();
    }
}

/// What the game would see after the merge stays within the platform contract.
void check_remote_sample(const detail::remote::RemoteInput& input) {
    platform::InputSample s;
    input.merge_into(s);
    const float axes[] = {s.circle_x, s.circle_y, s.cstick_x, s.cstick_y};
    for (float a : axes) {
        if (a < -1.0f || a > 1.0f) std::abort();
    }
    if ((s.buttons & ~detail::remote::kButtonMask) != 0) std::abort();
    if (s.touching && (s.touch_x < 0 || s.touch_x > 319 || s.touch_y < 0 || s.touch_y > 239)) std::abort();
}

} // namespace

int remote_target(const std::uint8_t* data, std::size_t size) {
    using namespace detail::remote;
    check_remote_round_trips(data, size);
    RemoteReader reader;
    RemoteSchedule schedule;
    RemoteInput input;
    ReadItem items[kScheduleCapacity];
    std::uint32_t now = 1, last_applied = 0;
    std::size_t pos = 0;
    while (pos < size) {
        const std::uint8_t control = data[pos++];
        std::size_t n = reader.bytes_wanted(schedule.room());
        if (n > (control & 0x3Fu)) n = control & 0x3Fu;
        if (n > size - pos) n = size - pos;
        const FeedResult fed = reader.feed(data + pos, n, now, items, schedule.room());
        if (fed.consumed != n || fed.items > kScheduleCapacity) std::abort(); // bounded reads always fit
        pos += n;
        for (std::size_t i = 0; i < fed.items; ++i) {
            if (items[i].reason != Reject::None || items[i].message.type == MessageType::Hello) continue;
            if (items[i].message.frame == 0 || !schedule.push(items[i].message, now)) std::abort();
        }
        Message due;
        while (schedule.pop_due(now, due)) {
            if (due.frame > now || due.frame < last_applied) std::abort(); // never early, always in order
            last_applied = due.frame;
            if (due.type == MessageType::Bye) input.release_all();
            input.apply(due);
        }
        check_remote_sample(input);
        now += control >> 6; // 0-3 frames pass
    }
    return 0;
}

int ogg_target(const std::uint8_t* data, std::size_t size) {
    // Open, decode everything, seek to the middle and to the loop point: never out of bounds, never
    // more frames than asked or than the file's length.
    alignas(16) static unsigned char arena[detail::OggDecoder::kDefaultArenaBytes];
    static detail::OggDecoder decoder;
    static std::int16_t frames[2 * 1024];
    FuzzBytes bytes{data, size};
    if (!decoder.open({&bytes, size, read_fuzz_bytes}, arena, sizeof arena)) return 0;
    for (int pass = 0; pass < 3; ++pass) {
        for (int chunk = 0; chunk < 48; ++chunk) { // ~2 s of audio per pass keeps libFuzzer fast
            const auto got = decoder.read(frames, 1024);
            if (!got || got.value() == 0) break;
            if (got.value() > 1024 || decoder.position() > decoder.length()) std::abort();
        }
        const std::uint64_t target = pass == 0 ? decoder.length() / 2 : decoder.loop_start();
        if (decoder.seek(target) && decoder.position() != target) std::abort();
    }
    decoder.close();
    return 0;
}

} // namespace nnh::fuzz
