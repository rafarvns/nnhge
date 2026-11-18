#include "audio/ogg_reader.hpp"

#include <cstring>

#include "audio/ogg_pages.hpp"
#include "audio/stb_vorbis_api.hpp"

namespace nnh::detail {

namespace {

std::int16_t to_pcm16(float v) {
    if (v >= 1.0f) return 32767;
    if (v <= -1.0f) return -32767;
    const float scaled = v * 32767.0f;
    return static_cast<std::int16_t>(scaled >= 0 ? scaled + 0.5f : scaled - 0.5f);
}

/// "LOOPSTART=<digits>" (key in any case). @return false if `comment` is something else.
bool parse_loop_start(const char* comment, std::uint64_t& out) {
    static const char kKey[] = "LOOPSTART=";
    for (std::size_t i = 0; i + 1 < sizeof kKey; ++i) {
        char c = comment[i];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if (c != kKey[i]) return false;
    }
    const char* digits = comment + sizeof kKey - 1;
    if (*digits == '\0') return false;
    std::uint64_t value = 0;
    for (; *digits != '\0'; ++digits) {
        if (*digits < '0' || *digits > '9' || value > 0xFFFFFFFFull) return false;
        value = value * 10 + static_cast<std::uint64_t>(*digits - '0');
    }
    out = value;
    return true;
}

} // namespace

Result<void> OggDecoder::open(const ByteSource& source, void* arena, std::size_t arena_size) {
    close();
    if (source.read_at == nullptr || arena == nullptr) return Error::make(ErrorCode::InvalidArgument, "no source or arena");
    source_ = source;
    arena_ = static_cast<unsigned char*>(arena);
    arena_size_ = arena_size;
    length_ = ogg_last_granule(source_, input_, sizeof input_);
    auto started = start_decoder();
    if (!started) {
        close();
        return started;
    }
    const stb_vorbis_info info = stb_vorbis_get_info(vorbis_);
    if (info.channels < 1 || info.channels > 2 || info.sample_rate < 8000 || info.sample_rate > 48000) {
        const int channels = info.channels;
        const unsigned rate = info.sample_rate;
        close();
        return Error::make(ErrorCode::Unsupported, "Ogg Vorbis %d channels %u Hz", channels, rate);
    }
    channels_ = info.channels;
    rate_ = info.sample_rate;
    arena_used_ = static_cast<std::size_t>(info.setup_memory_required) + info.temp_memory_required;
    read_tags();
    return {};
}

void OggDecoder::close() {
    if (vorbis_ != nullptr) stb_vorbis_close(vorbis_);
    vorbis_ = nullptr;
    channels_ = 0;
    rate_ = 0;
    length_ = 0;
    has_loop_start_ = false;
    loop_start_ = 0;
    has_cache_ = false;
    pending_count_ = pending_at_ = 0;
}

Result<void> OggDecoder::start_decoder() {
    if (vorbis_ != nullptr) stb_vorbis_close(vorbis_);
    vorbis_ = nullptr;
    file_at_ = 0;
    input_used_ = input_size_ = 0;
    source_done_ = false;
    pending_count_ = pending_at_ = 0;
    position_ = 0;
    const stb_vorbis_alloc alloc{reinterpret_cast<char*>(arena_), static_cast<int>(arena_size_)};
    for (;;) {
        auto more = fill_input();
        if (!more) return more.error();
        int used = 0;
        int error = 0;
        vorbis_ = stb_vorbis_open_pushdata(input_, static_cast<int>(input_size_), &used, &error, &alloc);
        if (vorbis_ != nullptr) {
            input_used_ = static_cast<std::size_t>(used);
            audio_start_ = static_cast<std::uint64_t>(used);
            return {};
        }
        if (error == VORBIS_outofmem) return Error::make(ErrorCode::OutOfMemory, "Ogg Vorbis needs a bigger arena");
        if (error == VORBIS_need_more_data && input_size_ == sizeof input_) {
            return Error::make(ErrorCode::Corrupted, "Ogg headers over %u KB (cover art?)", unsigned(sizeof input_ / 1024));
        }
        if (error != VORBIS_need_more_data || !more.value()) return Error::make(ErrorCode::Corrupted, "not an Ogg Vorbis file");
    }
}

Result<bool> OggDecoder::fill_input() {
    if (input_used_ > 0) {
        std::memmove(input_, input_ + input_used_, input_size_ - input_used_);
        input_size_ -= input_used_;
        input_used_ = 0;
    }
    if (file_at_ >= source_.size || input_size_ == sizeof input_) return false;
    const std::uint64_t left = source_.size - file_at_;
    const std::size_t room = sizeof input_ - input_size_;
    const std::size_t want = left < room ? static_cast<std::size_t>(left) : room;
    if (!read_exact(source_, file_at_, input_ + input_size_, want)) return Error::make(ErrorCode::IoError, "Ogg read failed");
    input_size_ += want;
    file_at_ += want;
    return true;
}

Result<bool> OggDecoder::decode_packet() {
    for (;;) {
        int channels = 0;
        float** output = nullptr;
        int samples = 0;
        const int used = stb_vorbis_decode_frame_pushdata(vorbis_, input_ + input_used_,
                                                          static_cast<int>(input_size_ - input_used_), &channels, &output,
                                                          &samples);
        if (used == 0) { // needs more bytes
            const bool full = input_used_ == 0 && input_size_ == sizeof input_;
            auto more = fill_input();
            if (!more) return more.error();
            if (!more.value()) {
                if (full) return Error::make(ErrorCode::Corrupted, "Vorbis packet larger than %u bytes", unsigned(sizeof input_));
                return false; // end of the file
            }
            continue;
        }
        input_used_ += static_cast<std::size_t>(used);
        if (samples > 0) {
            pending_ = output;
            pending_count_ = samples;
            pending_at_ = 0;
            return true;
        }
    }
}

Result<std::uint32_t> OggDecoder::read(std::int16_t* out, std::uint32_t capacity) {
    if (vorbis_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "Ogg decoder not open");
    std::uint32_t written = 0;
    while (written < capacity && position_ < length_) {
        if (pending_at_ == pending_count_) {
            auto decoded = decode_packet();
            if (!decoded) return decoded.error();
            if (!decoded.value()) break;
            continue;
        }
        std::uint64_t n = static_cast<std::uint64_t>(pending_count_ - pending_at_);
        if (n > capacity - written) n = capacity - written;
        if (n > length_ - position_) n = length_ - position_;
        for (std::uint64_t i = 0; i < n; ++i) {
            for (int c = 0; c < channels_; ++c) *out++ = to_pcm16(pending_[c][pending_at_ + static_cast<int>(i)]);
        }
        pending_at_ += static_cast<int>(n);
        position_ += n;
        written += static_cast<std::uint32_t>(n);
    }
    return written;
}

void OggDecoder::read_tags() {
    const stb_vorbis_comment comments = stb_vorbis_get_comment(vorbis_);
    for (int i = 0; i < comments.comment_list_length; ++i) {
        std::uint64_t value = 0;
        if (comments.comment_list[i] != nullptr && parse_loop_start(comments.comment_list[i], value) && value < length_) {
            loop_start_ = value;
            has_loop_start_ = true;
        }
    }
}

} // namespace nnh::detail
