// A batch on the wire (see batch_sender.hpp).

#include "reload/batch_sender.hpp"

#include "core/crc32.hpp"
#include "reload/reload_wire.hpp"

namespace nnh::tool::reload {
namespace {

namespace wire = nnh::detail::remote;

void put_message(std::vector<std::uint8_t>& out, const wire::Message& m) {
    std::uint8_t bytes[wire::kMessageSize];
    wire::encode_message(m, bytes);
    out.insert(out.end(), bytes, bytes + wire::kMessageSize);
}

constexpr const char* kTooMany = "too many changes for one reload: restart";

} // namespace

EncodedBatch encode_batch(std::uint16_t batch, const std::vector<BatchFileData>& files) {
    EncodedBatch e;
    if (files.empty() || files.size() > wire::kReloadMaxFiles) {
        e.error = files.empty() ? "nothing to send" : kTooMany;
        return e;
    }
    std::uint64_t total = 0;
    for (const BatchFileData& f : files) {
        if (f.path.empty() || f.path.size() > static_cast<std::size_t>(wire::kReloadMaxPath)) {
            e.error = "asset path too long for a reload: " + f.path;
            return e;
        }
        if (!f.removed && f.bytes.size() > wire::kReloadMaxFileBytes) {
            e.error = kTooMany;
            return e;
        }
        total += f.path.size() + (f.removed ? 0 : f.bytes.size());
    }
    if (total > wire::kReloadMaxBatchBytes) {
        e.error = kTooMany;
        return e;
    }
    e.files = static_cast<std::uint16_t>(files.size());
    e.total = static_cast<std::uint32_t>(total);
    put_message(e.bytes, wire::reload_begin_message(batch, e.files, e.total));
    for (std::size_t i = 0; i < files.size(); ++i) {
        const BatchFileData& f = files[i];
        const auto index = static_cast<std::uint16_t>(i);
        const auto len = static_cast<std::uint8_t>(f.path.size());
        if (f.removed) {
            put_message(e.bytes, wire::reload_file_message(index, wire::ReloadOp::Remove, len, 0));
            e.bytes.insert(e.bytes.end(), f.path.begin(), f.path.end());
            continue;
        }
        const auto size = static_cast<std::uint32_t>(f.bytes.size());
        put_message(e.bytes, wire::reload_file_message(index, wire::ReloadOp::Put, len, size));
        e.bytes.insert(e.bytes.end(), f.path.begin(), f.path.end());
        const std::uint32_t crc = nnh::detail::crc32(f.bytes.data(), f.bytes.size());
        for (int b = 0; b < 4; ++b) e.bytes.push_back(static_cast<std::uint8_t>(crc >> (8 * b)));
        for (std::uint32_t at = 0; at < size; at += wire::kReloadMaxChunk) {
            const std::uint32_t n = size - at < wire::kReloadMaxChunk ? size - at : wire::kReloadMaxChunk;
            put_message(e.bytes, wire::reload_chunk_message(index, static_cast<std::uint16_t>(n), at));
            e.bytes.insert(e.bytes.end(), f.bytes.begin() + at, f.bytes.begin() + at + n);
        }
    }
    return e;
}

std::vector<std::uint8_t> encode_apply(std::uint16_t batch, std::uint16_t files, std::uint32_t frame, std::uint16_t seq) {
    std::vector<std::uint8_t> out;
    wire::Message m = wire::reload_apply_message(batch, files, frame);
    m.seq = seq;
    put_message(out, m);
    return out;
}

std::vector<std::uint8_t> encode_cancel(std::uint16_t batch) {
    std::vector<std::uint8_t> out;
    put_message(out, wire::reload_cancel_message(batch));
    return out;
}

std::vector<std::uint8_t> encode_notice(bool needs_restart, const std::string& source, std::uint16_t others) {
    const std::string path = source.substr(0, static_cast<std::size_t>(wire::kReloadMaxPath));
    std::vector<std::uint8_t> out;
    if (path.empty()) return out;
    put_message(out, wire::reload_notice_message(needs_restart ? wire::NoticeReason::NeedsRestart
                                                               : wire::NoticeReason::ConversionFailed,
                                                 static_cast<std::uint8_t>(path.size()), others));
    out.insert(out.end(), path.begin(), path.end());
    return out;
}

} // namespace nnh::tool::reload
