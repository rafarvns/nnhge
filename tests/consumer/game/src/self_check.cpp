#include "self_check.hpp"

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <nnh/data.hpp>
#include <nnh/texture.hpp>

#include "data_types.hpp"

namespace {

/// A CRC-32 (zlib's) of values written one by one, little-endian, whatever the system.
class Sum {
public:
    void byte(std::uint8_t b) {
        crc_ ^= b;
        for (int k = 0; k < 8; ++k) crc_ = (crc_ >> 1) ^ (0xEDB88320u & (0u - (crc_ & 1u)));
    }
    void u32(std::uint32_t v) {
        for (int k = 0; k < 4; ++k) byte(static_cast<std::uint8_t>(v >> (8 * k)));
    }
    void i32(int v) { u32(static_cast<std::uint32_t>(v)); }
    void text(const char* s) {
        const std::size_t n = std::strlen(s);
        u32(static_cast<std::uint32_t>(n));
        for (std::size_t i = 0; i < n; ++i) byte(static_cast<std::uint8_t>(s[i]));
    }
    std::uint32_t value() const { return ~crc_; }

private:
    std::uint32_t crc_ = 0xFFFFFFFFu;
};

/// An image from ASSETS (art/hero.png): its size.
const char* check_texture(Sum& sum) {
    auto hero = nnh::load_texture("art/hero");
    if (!hero) return "art/hero doesn't load";
    sum.i32(hero.value().width());
    sum.i32(hero.value().height());
    return "";
}

/// A data asset of the game's own schema (things/lamp.thing), its texture reference followed.
const char* check_data(Sum& sum) {
    Thing lamp;
    if (!nnh::data::load("things/lamp", lamp)) return "things/lamp doesn't load";
    if (nnh::data::count<Thing>() != 1) return "wrong number of things";
    sum.text(lamp.name.c_str());
    sum.i32(lamp.value);
    sum.text(nnh::data::path(lamp.sprite));
    return "";
}

} // namespace

ConsumerCheck run_consumer_check() {
    ConsumerCheck r;
    Sum sum;
    const char* failed = "";
    for (const char* (*step)(Sum&) : {check_texture, check_data}) {
        if (failed[0] == '\0') failed = step(sum);
    }
    std::snprintf(r.failure, sizeof r.failure, "%s", failed);
    r.pass = failed[0] == '\0';
    r.checksum = sum.value();
    return r;
}

void format_consumer_check(const ConsumerCheck& r, char* out, std::size_t size) {
    if (r.pass) {
        std::snprintf(out, size, "consumer: PASS %08X", static_cast<unsigned>(r.checksum));
    } else {
        std::snprintf(out, size, "consumer: FAIL %s", r.failure);
    }
}
