#include "audio/wav_stream.hpp"

#include "audio/ogg_pages.hpp"
#include "audio/sound_decode.hpp"

namespace nnh::detail {

Result<void> WavStream::open(const ByteSource& source) {
    source_ = source;
    position_ = length_ = 0;
    const std::size_t header = source.size < sizeof buffer_ ? static_cast<std::size_t>(source.size) : sizeof buffer_;
    if (!read_exact(source, 0, buffer_, header)) return Error::make(ErrorCode::IoError, "WAV read failed");
    auto parsed = parse_wav(buffer_, header, static_cast<std::size_t>(source.size));
    if (!parsed) return parsed.error();
    format_ = parsed.value();
    length_ = format_.frames();
    return {};
}

Result<std::uint32_t> WavStream::read(std::int16_t* out, std::uint32_t capacity) {
    const std::size_t frame_bytes = static_cast<std::size_t>(format_.channels * format_.bits / 8);
    const std::uint64_t left = length_ - position_;
    std::uint32_t total = capacity < left ? capacity : static_cast<std::uint32_t>(left);
    std::uint32_t done = 0;
    while (done < total) {
        std::uint32_t n = total - done;
        const std::uint32_t fit = static_cast<std::uint32_t>(sizeof buffer_ / frame_bytes);
        if (n > fit) n = fit;
        const std::uint64_t offset = format_.data_offset + position_ * frame_bytes;
        if (!read_exact(source_, offset, buffer_, n * frame_bytes)) return Error::make(ErrorCode::IoError, "WAV read failed");
        // Convert this piece as if it were a whole file starting at buffer_.
        WavFormat piece = format_;
        piece.data_offset = 0;
        piece.data_size = n * frame_bytes;
        wav_to_pcm16(buffer_, piece, out + static_cast<std::size_t>(done) * static_cast<std::size_t>(format_.channels));
        done += n;
        position_ += n;
    }
    return done;
}

Result<void> WavStream::seek(std::uint64_t frame) {
    if (frame > length_) return Error::make(ErrorCode::InvalidArgument, "seek past the end of the WAV");
    position_ = frame;
    return {};
}

} // namespace nnh::detail
