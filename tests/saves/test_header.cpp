// The structured header of a structured save (spec 027 T1.1): hand-written headers accepted, each field wrong
// refused with its message, a newer format and a raw payload recognised.

#include <cstdint>
#include <cstring>
#include <string_view>

#include "nnh/detail/name_hash.hpp"
#include "saves/save_header.hpp"
#include "test.hpp"

namespace {

using nnh::ErrorCode;

// "NNSV", format 1, key length 8, flags 0, version 3, FNV-1a 32 of "progress" (filled in by the constructor),
// then "progress".
struct Bytes {
    std::uint8_t b[32] = {'N', 'N', 'S', 'V', 1, 8, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0,
                          'p', 'r', 'o', 'g', 'r', 'e', 's', 's'};
    Bytes() {
        const std::uint32_t h = nnh::detail::fnv1a32("progress");
        b[12] = static_cast<std::uint8_t>(h);
        b[13] = static_cast<std::uint8_t>(h >> 8);
        b[14] = static_cast<std::uint8_t>(h >> 16);
        b[15] = static_cast<std::uint8_t>(h >> 24);
    }
};

bool refused(const Bytes& bytes, ErrorCode code, const char* message, std::size_t size = 24) {
    const auto r = nnh::detail::parse_save_struct_header(bytes.b, size);
    return !r.ok() && r.error().code == code && std::strstr(r.error().message, message) != nullptr;
}

} // namespace

NNH_TEST(save_header_builds_the_documented_bytes) {
    std::uint8_t out[40] = {};
    CHECK(nnh::detail::build_save_struct_header(out, "progress", 3) == 24);
    CHECK(std::memcmp(out, Bytes{}.b, 24) == 0);
}

NNH_TEST(save_header_parses_a_hand_written_header) {
    const auto r = nnh::detail::parse_save_struct_header(Bytes{}.b, 24);
    CHECK(r.ok());
    CHECK(r.value().version == 3 && r.value().format == 1 && r.value().key_length == 8);
    CHECK(std::string_view(r.value().key) == "progress" && r.value().size == 24);
    CHECK(r.value().key_hash == nnh::detail::fnv1a32("progress"));
}

NNH_TEST(save_header_recognises_a_raw_payload) {
    Bytes b;
    b.b[3] = 'S'; // "NNSS"
    CHECK(refused(b, ErrorCode::Unsupported, "raw nnh::save file"));
    CHECK(refused(b, ErrorCode::Unsupported, "raw nnh::save file", 2)); // too short to hold the magic
}

NNH_TEST(save_header_refuses_a_newer_format_as_unsupported) {
    Bytes b;
    b.b[4] = 2;
    CHECK(refused(b, ErrorCode::Unsupported, "format 2; this engine reads 1"));
}

NNH_TEST(save_header_refuses_each_damaged_field) {
    Bytes b;
    b.b[4] = 0;
    CHECK(refused(b, ErrorCode::Corrupted, "format 0"));
    b = Bytes{};
    b.b[5] = 0;
    CHECK(refused(b, ErrorCode::Corrupted, "key length 0"));
    b = Bytes{};
    b.b[5] = 17;
    CHECK(refused(b, ErrorCode::Corrupted, "key length 17"));
    b = Bytes{};
    b.b[6] = 1;
    CHECK(refused(b, ErrorCode::Corrupted, "flags"));
    b = Bytes{};
    b.b[8] = 0;
    CHECK(refused(b, ErrorCode::Corrupted, "save version 0"));
    b = Bytes{};
    b.b[16] = 'P';
    CHECK(refused(b, ErrorCode::Corrupted, "bad key"));
    b = Bytes{};
    b.b[16] = '7';
    CHECK(refused(b, ErrorCode::Corrupted, "bad key"));
    b = Bytes{};
    b.b[12] ^= 1;
    CHECK(refused(b, ErrorCode::Corrupted, "key hash"));
    b = Bytes{};
    CHECK(refused(b, ErrorCode::Corrupted, "header cut short", 23));
    CHECK(refused(b, ErrorCode::Corrupted, "header cut short", 10));
}

NNH_TEST(save_keys_follow_the_data_asset_rule) {
    using nnh::detail::save_key_valid;
    CHECK(save_key_valid("progress") && save_key_valid("a") && save_key_valid("slot_2"));
    CHECK(save_key_valid("abcdefghijklmnop"));   // 16
    CHECK(!save_key_valid("abcdefghijklmnopq")); // 17
    CHECK(!save_key_valid("") && !save_key_valid("2nd") && !save_key_valid("_x") && !save_key_valid("Prog"));
    CHECK(!save_key_valid("a-b") && !save_key_valid("a b"));
}
