// Hot reload messages and records (see reload_wire.hpp). Pure: only the standard library, shared with tools/nnh.

#include "reload/reload_wire.hpp"

namespace nnh::detail::remote {
namespace {

std::uint16_t get16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

std::uint32_t get32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

void put16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}

void put32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}

bool zero(const std::uint8_t* a, int from, int to) {
    for (int i = from; i < to; ++i) {
        if (a[i] != 0) return false;
    }
    return true;
}

bool files_ok(std::uint16_t files) { return files >= 1 && files <= kReloadMaxFiles; }

/// Bytes 8–15 of a reload message (frame already checked) into `out`.
bool decode_fields(const std::uint8_t* a, Message& out) {
    switch (out.type) {
    case MessageType::ReloadBegin: {
        const std::uint16_t files = get16(a + 2);
        out.id = get16(a);
        out.x = static_cast<std::int16_t>(files);
        out.press = get32(a + 4);
        return out.id != 0 && files_ok(files) && out.press <= kReloadMaxBatchBytes;
    }
    case MessageType::ReloadFile:
        out.id = get16(a);
        out.screens = a[2];
        out.x = a[3];
        out.press = get32(a + 4);
        if (out.id >= kReloadMaxFiles || a[3] == 0) return false;
        if (a[2] == static_cast<std::uint8_t>(ReloadOp::Put)) return out.press <= kReloadMaxFileBytes;
        return a[2] == static_cast<std::uint8_t>(ReloadOp::Remove) && out.press == 0;
    case MessageType::ReloadChunk: {
        const std::uint16_t length = get16(a + 2);
        out.id = get16(a);
        out.x = static_cast<std::int16_t>(length);
        out.press = get32(a + 4);
        return out.id < kReloadMaxFiles && length >= 1 && length <= kReloadMaxChunk && out.press < kReloadMaxFileBytes;
    }
    case MessageType::ReloadApply: {
        const std::uint16_t files = get16(a + 2);
        out.id = get16(a);
        out.x = static_cast<std::int16_t>(files);
        return out.id != 0 && files_ok(files) && zero(a, 4, 8);
    }
    case MessageType::ReloadCancel: out.id = get16(a); return zero(a, 2, 8);
    case MessageType::ReloadNotice:
        out.screens = a[0];
        out.x = a[1];
        out.release = get16(a + 2);
        return a[0] >= 1 && a[0] <= 2 && a[1] != 0 && zero(a, 4, 8);
    default: return false;
    }
}

bool stage_ok(std::uint8_t stage) { return stage >= 1 && stage <= static_cast<std::uint8_t>(ReloadStage::Notice); }

} // namespace

Message reload_begin_message(std::uint16_t batch, std::uint16_t files, std::uint32_t total) {
    Message m;
    m.type = MessageType::ReloadBegin;
    m.id = batch;
    m.x = static_cast<std::int16_t>(files);
    m.press = total;
    return m;
}

Message reload_file_message(std::uint16_t index, ReloadOp op, std::uint8_t path_length, std::uint32_t size) {
    Message m;
    m.type = MessageType::ReloadFile;
    m.id = index;
    m.screens = static_cast<std::uint8_t>(op);
    m.x = path_length;
    m.press = size;
    return m;
}

Message reload_chunk_message(std::uint16_t index, std::uint16_t length, std::uint32_t offset) {
    Message m;
    m.type = MessageType::ReloadChunk;
    m.id = index;
    m.x = static_cast<std::int16_t>(length);
    m.press = offset;
    return m;
}

Message reload_apply_message(std::uint16_t batch, std::uint16_t files, std::uint32_t frame) {
    Message m;
    m.type = MessageType::ReloadApply;
    m.frame = frame;
    m.id = batch;
    m.x = static_cast<std::int16_t>(files);
    return m;
}

Message reload_cancel_message(std::uint16_t batch) {
    Message m;
    m.type = MessageType::ReloadCancel;
    m.id = batch;
    return m;
}

Message reload_notice_message(NoticeReason reason, std::uint8_t path_length, std::uint16_t others) {
    Message m;
    m.type = MessageType::ReloadNotice;
    m.screens = static_cast<std::uint8_t>(reason);
    m.x = path_length;
    m.release = others;
    return m;
}

bool is_reload_transfer(MessageType type) {
    return type == MessageType::ReloadBegin || type == MessageType::ReloadFile || type == MessageType::ReloadChunk ||
           type == MessageType::ReloadCancel || type == MessageType::ReloadNotice;
}

std::uint32_t payload_length(const Message& m) {
    switch (m.type) {
    case MessageType::ReloadFile:
        return reload_path_length(m) + (reload_op(m) == ReloadOp::Put ? 4u : 0u);
    case MessageType::ReloadChunk: return reload_chunk_length(m);
    case MessageType::ReloadNotice: return reload_path_length(m);
    default: return 0;
    }
}

bool decode_reload_arguments(const std::uint8_t* in, Message& out) {
    if (is_reload_transfer(out.type) && out.frame != 0) return false;
    return decode_fields(in + 8, out);
}

void encode_reload_arguments(const Message& m, std::uint8_t* a) {
    switch (m.type) {
    case MessageType::ReloadBegin:
    case MessageType::ReloadChunk:
        put16(a, m.id);
        put16(a + 2, static_cast<std::uint16_t>(m.x));
        put32(a + 4, m.press);
        break;
    case MessageType::ReloadFile:
        put16(a, m.id);
        a[2] = m.screens;
        a[3] = static_cast<std::uint8_t>(m.x);
        put32(a + 4, m.press);
        break;
    case MessageType::ReloadApply:
        put16(a, m.id);
        put16(a + 2, static_cast<std::uint16_t>(m.x));
        break;
    case MessageType::ReloadCancel: put16(a, m.id); break;
    case MessageType::ReloadNotice:
        a[0] = m.screens;
        a[1] = static_cast<std::uint8_t>(m.x);
        put16(a + 2, static_cast<std::uint16_t>(m.release));
        break;
    default: break;
    }
}

std::size_t encode_reload_staged(const ReloadStagedRecord& r, std::uint8_t* out) {
    encode_record_header(RecordType::ReloadStaged, kReloadStagedPayload, out);
    std::uint8_t* p = out + kRecordHeaderSize;
    put16(p, r.batch);
    put16(p + 2, r.files);
    put32(p + 4, r.frame);
    return kRecordHeaderSize + kReloadStagedPayload;
}

std::size_t encode_reload_applied(const ReloadAppliedRecord& r, std::uint8_t* out) {
    encode_record_header(RecordType::ReloadApplied, kReloadAppliedPayload, out);
    std::uint8_t* p = out + kRecordHeaderSize;
    put16(p, r.batch);
    put16(p + 2, r.files);
    put32(p + 4, r.frame);
    put32(p + 8, r.count);
    put32(p + 12, r.micros);
    return kRecordHeaderSize + kReloadAppliedPayload;
}

std::size_t encode_reload_refused(const ReloadRefusedRecord& r, std::uint8_t* out) {
    encode_record_header(RecordType::ReloadRefused, kReloadRefusedPayload, out);
    std::uint8_t* p = out + kRecordHeaderSize;
    put16(p, r.batch);
    p[2] = static_cast<std::uint8_t>(r.stage);
    p[3] = static_cast<std::uint8_t>(r.reason);
    put16(p + 4, r.file);
    put16(p + 6, 0);
    put32(p + 8, r.frame);
    put32(p + 12, r.missing);
    return kRecordHeaderSize + kReloadRefusedPayload;
}

bool decode_reload_staged(const std::uint8_t* p, std::uint32_t length, ReloadStagedRecord& out) {
    if (length != kReloadStagedPayload) return false;
    out.batch = get16(p);
    out.files = get16(p + 2);
    out.frame = get32(p + 4);
    return true;
}

bool decode_reload_applied(const std::uint8_t* p, std::uint32_t length, ReloadAppliedRecord& out) {
    if (length != kReloadAppliedPayload) return false;
    out.batch = get16(p);
    out.files = get16(p + 2);
    out.frame = get32(p + 4);
    out.count = get32(p + 8);
    out.micros = get32(p + 12);
    return true;
}

bool decode_reload_refused(const std::uint8_t* p, std::uint32_t length, ReloadRefusedRecord& out) {
    if (length != kReloadRefusedPayload || !stage_ok(p[2]) || get16(p + 6) != 0) return false;
    out.batch = get16(p);
    out.stage = static_cast<ReloadStage>(p[2]);
    out.reason = static_cast<ReloadReason>(p[3]);
    out.file = get16(p + 4);
    out.frame = get32(p + 8);
    out.missing = get32(p + 12);
    return true;
}

bool reload_record_length_ok(RecordType type, std::uint32_t length) {
    switch (type) {
    case RecordType::ReloadStaged: return length == kReloadStagedPayload;
    case RecordType::ReloadApplied: return length == kReloadAppliedPayload;
    case RecordType::ReloadRefused: return length == kReloadRefusedPayload;
    default: return false;
    }
}

const char* reload_reason_text(ReloadReason reason) {
    switch (reason) {
    case ReloadReason::None: return "none";
    case ReloadReason::Order: return "out of order";
    case ReloadReason::Offset: return "chunk at the wrong offset";
    case ReloadReason::Size: return "file size differs from its announcement";
    case ReloadReason::Total: return "batch total differs from its announcement";
    case ReloadReason::Crc: return "bad CRC";
    case ReloadReason::Path: return "invalid asset path";
    case ReloadReason::Batch: return "no such batch staged";
    case ReloadReason::Write: return "could not write the file";
    case ReloadReason::Disconnected: return "the PC disconnected";
    case ReloadReason::Corrupted: return "corrupted";
    case ReloadReason::Consistency: return "files don't match the batch's asset index";
    case ReloadReason::NoMemory: return "out of memory";
    case ReloadReason::Schema: return "made for another struct: rebuild and restart";
    case ReloadReason::Languages: return "another language list: rebuild and restart";
    case ReloadReason::TooManyPaths: return "too many reloaded paths this session: restart";
    case ReloadReason::ConversionFailed: return "conversion failed (see nnh)";
    case ReloadReason::NeedsRestart: return "needs a rebuild and restart";
    }
    return "?";
}

} // namespace nnh::detail::remote
