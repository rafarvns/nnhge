// Fuzz target bodies (see fuzz_targets.hpp).

#include "fuzz_targets.hpp"

#include <cstdlib>
#include <cstring>

#include "assets/asset_index.hpp"
#include "audio/ogg_reader.hpp"
#include "audio/wav_reader.hpp"
#include "fs/save_format.hpp"
#include "nnh/fs.hpp"

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

} // namespace

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
