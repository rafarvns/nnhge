#pragma once
// Console remote wire format (spec 013, D28): the fixed 16-byte messages the PC sends to a Development game,
// and the header of the records the game writes into its log stream. Pure: no engine includes, so the host
// tool `nnh` (tools/nnh) compiles this file too, like core/crc32.cpp. All integers little-endian.
// Protocol: specs/013-console-remote/plan.md#protocol. Tests: tests/remote/test_remote_wire.cpp.

#include <cstddef>
#include <cstdint>

namespace nnh::detail::remote {

inline constexpr std::uint16_t kVersion = 1;

// ---- PC → console: messages ----------------------------------------------------------------------------

inline constexpr std::size_t kMessageSize = 16;
inline constexpr std::uint8_t kMessageMagic = 0x52; // 'R'
/// The 14 nnh::Button bits (A … Right). Checked against <nnh/input.hpp> in remote_input.cpp.
inline constexpr std::uint32_t kButtonMask = 0x3FFFu;
/// Stick positions are thousandths of the full range, per axis.
inline constexpr std::int16_t kStickMax = 1000;
inline constexpr std::uint16_t kTouchMaxX = 319;
inline constexpr std::uint16_t kTouchMaxY = 239;
inline constexpr std::uint8_t kScreenTop = 1u << 0;
inline constexpr std::uint8_t kScreenBottom = 1u << 1;
/// Screenshot: the top screen's right eye too (spec 026, plan P20): a Screen record with screen 2; a frame drawn with
/// one eye answers with the top screen as it is (both eyes see it).
inline constexpr std::uint8_t kScreenTopRight = 1u << 2;
inline constexpr std::uint8_t kScreenMask = kScreenTop | kScreenBottom | kScreenTopRight;
/// The 3D slider (spec 026 FR-16) in thousandths. On the wire a Slider message is a release flag (byte 8) and the value
/// (bytes 10–11); decoded, a release is Message::slider == kSliderRelease (the message stays 32 bytes).
inline constexpr std::uint16_t kSliderMax = 1000;
inline constexpr std::uint16_t kSliderRelease = 0xFFFF;
/// Hello flag: the PC runs a script and owns the controls; until the Bye (or the connection ends) the game reads
/// only the PC's input, never the console's own (spec 013 amendment 2026-11-24). Interactive control leaves it 0.
inline constexpr std::uint16_t kHelloExclusive = 1u << 0;
/// Every Hello flag this version knows; any other bit is Reject::Range.
inline constexpr std::uint16_t kHelloFlags = kHelloExclusive;

/// Slider (spec 026) and Language (spec 029) are additive in version 1: an older game rejects them as Reject::Type.
enum class MessageType : std::uint8_t {
    Hello = 1, Buttons = 2, Stick = 3, Touch = 4, Screenshot = 5, Bye = 6, Slider = 7, Language = 8,
};

/// The system language (spec 029 FR-27): bytes 8–15 hold a well-formed code (system/language_tag.hpp) NUL-padded, or are
/// all zero for `language off`. Decoded, the 8 bytes sit in Message::press (bytes 8–11) and Message::release (12–15),
/// little-endian, so the message stays 32 bytes: pack_language() / unpack_language().
inline constexpr int kLanguageCodeBytes = 8;

/// Why a message was refused (the `reason` of a `Rejected` record). A non-zero reserved byte is `Range`.
enum class Reject : std::uint8_t {
    None = 0, ///< accepted
    Magic = 1,
    Type = 2,
    Range = 3,
    Order = 4,
    NotAttached = 5,
    TooMany = 6,
};

/// One decoded message. Only the fields of its `type` are meaningful; the others stay 0.
struct Message {
    MessageType type = MessageType::Hello;
    std::uint16_t seq = 0;   ///< set by the PC, echoed in Late/Rejected/Dropped
    std::uint32_t frame = 0; ///< apply at the start of this frame; 0 = the next frame
    std::uint16_t version = 0;                     ///< Hello
    bool exclusive = false;                        ///< Hello: flag kHelloExclusive (a script owns the controls)
    std::uint32_t press = 0;                       ///< Buttons: bits to hold (kButtonMask only)
    std::uint32_t release = 0;                     ///< Buttons: bits to let go (disjoint from press)
    std::uint8_t stick = 0;                        ///< Stick: 0 Circle Pad, 1 C-Stick
    bool down = false;                             ///< Touch: touching
    std::int16_t x = 0;                            ///< Stick: −1000…1000; Touch: 0…319
    std::int16_t y = 0;                            ///< Stick: −1000…1000 (up); Touch: 0…239
    std::uint16_t id = 0;                          ///< Screenshot
    std::uint8_t screens = 0;                      ///< Screenshot: bits of kScreenMask, ≠ 0
    bool quit = false;                             ///< Bye
    std::uint16_t slider = 0;                      ///< Slider: 0…kSliderMax thousandths, or kSliderRelease
};

/// The frame a message applies to: its own, or for frame 0 ("the next frame") `now`, the frame whose start is
/// processing it (the first frame that can still see it).
constexpr std::uint32_t resolve_frame(std::uint32_t frame, std::uint32_t now) { return frame == 0 ? now : frame; }

/// Puts `code` (nullptr or "": off) into a Language message's press/release. @return false (the message unchanged) for
/// an ill-formed code or one longer than kLanguageCodeBytes.
bool pack_language(const char* code, Message& message);

/// The code of a decoded Language message into `out` (NUL-terminated; "" for off). @return false for off.
bool unpack_language(const Message& message, char out[kLanguageCodeBytes + 1]);

/// Writes `message` as 16 bytes (unused bytes 0). Never allocates.
void encode_message(const Message& message, std::uint8_t out[kMessageSize]);

/// Decodes and validates 16 bytes: magic, type, every argument's range, unused bytes 0, Buttons masks within
/// kButtonMask and disjoint, Hello version == kVersion with no flag outside kHelloFlags. @return Reject::None and fills `out`, or
/// the reason (then `out` is unspecified). A decoded message encodes back to the same 16 bytes.
Reject decode_message(const std::uint8_t in[kMessageSize], Message& out);

/// The `seq` field of 16 bytes, valid or not (to answer a rejected message).
std::uint16_t message_seq(const std::uint8_t in[kMessageSize]);

// ---- console → PC: records in the log stream ------------------------------------------------------------

/// Header: 0x1E 'N' 'R' type, then the u32 payload length. Log lines never contain 0x1E.
inline constexpr std::size_t kRecordHeaderSize = 8;
inline constexpr std::uint8_t kRecordMark[3] = {0x1E, 'N', 'R'};
/// Upper bound for any payload (a Screen record is at most 400 × 240 × 4 + its 12-byte header).
inline constexpr std::uint32_t kMaxRecordPayload = 1u << 20;

enum class RecordType : std::uint8_t {
    Hello = 1,
    Tick = 2,
    Late = 3,
    Rejected = 4,
    Dropped = 5,
    Screen = 6,
    ScreenFailed = 7,
};

/// Payload sizes: exact for every type but Screen, whose fixed part is followed by the pixels.
inline constexpr std::uint32_t kHelloPayload = 24;   // u16 version, u16 flags, u32 frame, u32 caps, char[12]
inline constexpr std::uint32_t kTickPayload = 4;     // u32 frame
inline constexpr std::uint32_t kLatePayload = 10;    // u16 seq, u32 wanted, u32 applied
inline constexpr std::uint32_t kRejectedPayload = 3; // u16 seq, u8 reason
inline constexpr std::uint32_t kDroppedPayload = 2;  // u16 seq
inline constexpr std::uint32_t kScreenFixed = 12;    // u16 id, u8 screen, u8 layout, u32 frame, u16 w, u16 h
inline constexpr std::uint32_t kScreenFailedPayload = 7; // u16 id, u8 screen, u32 frame

struct RecordHeader {
    RecordType type = RecordType::Tick;
    std::uint32_t length = 0; ///< payload bytes after the header
};

/// Writes a record header. Never allocates.
void encode_record_header(RecordType type, std::uint32_t length, std::uint8_t out[kRecordHeaderSize]);

/// Reads a record header: the mark, a known type and a length allowed for it (exact for fixed records;
/// kScreenFixed…kMaxRecordPayload for Screen). @return false if these bytes are not a record (text).
bool decode_record_header(const std::uint8_t in[kRecordHeaderSize], RecordHeader& out);

// ---- console → PC: whole records (header + payload), written by the console's session ---------------------

/// Bytes the largest fixed record takes (Hello), header included: the size of every encoder's `out`.
inline constexpr std::size_t kMaxFixedRecord = kRecordHeaderSize + kHelloPayload;

/// The fixed part of a Screen record; `width × height × bpp` pixel bytes follow it.
struct ScreenHeader {
    std::uint16_t id = 0;
    std::uint8_t screen = 0; ///< 0 top, 1 bottom, 2 the top screen's right eye (spec 026)
    std::uint8_t layout = 0; ///< platform::PixelLayout: 1 RGBA8 rows, 2 BGR8 columns bottom-up
    std::uint32_t frame = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
};

/// Each writes one record into `out` (kMaxFixedRecord bytes) and returns its size. Never allocate.
/// Hello: version kVersion, flags 0, the frame now, capabilities, flavor NUL-padded (cut) to 12 bytes.
std::size_t encode_hello_record(std::uint32_t frame, std::uint32_t capabilities, const char* flavor,
                                std::uint8_t* out);
std::size_t encode_tick_record(std::uint32_t frame, std::uint8_t* out);
std::size_t encode_late_record(std::uint16_t seq, std::uint32_t wanted, std::uint32_t applied, std::uint8_t* out);
std::size_t encode_rejected_record(std::uint16_t seq, Reject reason, std::uint8_t* out);
std::size_t encode_dropped_record(std::uint16_t seq, std::uint8_t* out);
/// The header and fixed part of a Screen record whose `pixel_bytes` are sent right after it.
std::size_t encode_screen_record(const ScreenHeader& header, std::uint32_t pixel_bytes, std::uint8_t* out);
std::size_t encode_screen_failed_record(std::uint16_t id, std::uint8_t screen, std::uint32_t frame,
                                        std::uint8_t* out);

} // namespace nnh::detail::remote
