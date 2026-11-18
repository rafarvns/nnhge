// OggDecoder::seek (spec 008 FR-3, loop points): restart decoding at the last page that ends before the
// target, let stb_vorbis find its position, then drop frames up to the target — sample-exact, without
// decoding the file from the start. The page found is cached: a loop point is sought on every loop.

#include "audio/ogg_pages.hpp"
#include "audio/ogg_reader.hpp"
#include "audio/stb_vorbis_api.hpp"

namespace nnh::detail {

Result<std::uint64_t> OggDecoder::find_page_before(std::uint64_t frame, std::uint64_t& granule) {
    if (has_cache_ && cached_frame_ == frame) {
        granule = cached_granule_;
        return cached_offset_;
    }
    std::uint64_t best = 0; // 0 = none: the target is in the first audio page
    granule = 0;
    OggPage page;
    for (std::uint64_t at = audio_start_; at < source_.size && read_ogg_page(source_, at, page); at += page.size) {
        if (page.granule < 0) continue;
        if (static_cast<std::uint64_t>(page.granule) > frame) break;
        best = at;
        granule = static_cast<std::uint64_t>(page.granule);
    }
    cached_frame_ = frame;
    cached_offset_ = best;
    cached_granule_ = granule;
    has_cache_ = true;
    return best;
}

Result<void> OggDecoder::skip_frames(std::uint64_t count) {
    while (count > 0) {
        if (pending_at_ == pending_count_) {
            auto decoded = decode_packet();
            if (!decoded) return decoded.error();
            if (!decoded.value()) { // damaged: the data ends before the length its last page promised
                return Error::make(ErrorCode::Corrupted, "Ogg data ends at frame %llu",
                                   static_cast<unsigned long long>(position_));
            }
            continue;
        }
        std::uint64_t n = static_cast<std::uint64_t>(pending_count_ - pending_at_);
        if (n > count) n = count;
        pending_at_ += static_cast<int>(n);
        position_ += n;
        count -= n;
    }
    return {};
}

Result<void> OggDecoder::seek(std::uint64_t frame) {
    if (vorbis_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "Ogg decoder not open");
    if (frame > length_) return Error::make(ErrorCode::InvalidArgument, "seek past the end of the file");
    if (frame == position_) return {};
    std::uint64_t granule = 0;
    auto page = find_page_before(frame, granule);
    if (!page) return page.error();
    if (page.value() != 0) {
        // Resume at that page; frames decoded before stb_vorbis knows its position end at or before
        // `granule` <= frame, so dropping them loses nothing.
        stb_vorbis_flush_pushdata(vorbis_);
        file_at_ = page.value();
        input_used_ = input_size_ = 0;
        pending_count_ = pending_at_ = 0;
        for (;;) {
            auto decoded = decode_packet();
            if (!decoded) return decoded.error();
            if (!decoded.value()) break;
            const int known = stb_vorbis_get_sample_offset(vorbis_);
            if (known < 0) continue;
            pending_count_ = pending_at_ = 0; // these frames end where `known` starts
            position_ = static_cast<std::uint64_t>(known);
            if (position_ <= frame) return skip_frames(frame - position_);
            break; // overshot (unexpected page layout): fall back to decoding from the start
        }
    }
    auto restarted = start_decoder();
    if (!restarted) return restarted;
    return skip_frames(frame);
}

} // namespace nnh::detail
