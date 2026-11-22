#include "check.hpp"

#include <cstdio>
#include <initializer_list>
#include <cstring>
#include <nnh/data.hpp>
#include <nnh/memory.hpp>

#include "data_types.hpp"

namespace data = nnh::data;

namespace {

/// A CRC-32 (zlib's) of values written one by one, little-endian, whatever the system.
class Sum {
public:
    void byte(std::uint8_t b) {
        crc_ ^= b;
        for (int k = 0; k < 8; ++k) crc_ = (crc_ >> 1) ^ (0xEDB88320u & (0u - (crc_ & 1u)));
    }
    void u16(std::uint32_t v) {
        byte(static_cast<std::uint8_t>(v));
        byte(static_cast<std::uint8_t>(v >> 8));
    }
    void u32(std::uint32_t v) {
        u16(v & 0xFFFF);
        u16(v >> 16);
    }
    void f32(float f) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &f, sizeof bits);
        u32(bits);
    }
    void text(const char* s) {
        const std::size_t n = std::strlen(s);
        u16(static_cast<std::uint32_t>(n));
        for (std::size_t i = 0; i < n; ++i) byte(static_cast<std::uint8_t>(s[i]));
    }
    void color(nnh::Color c) {
        byte(c.r);
        byte(c.g);
        byte(c.b);
        byte(c.a);
    }
    std::uint32_t value() const { return ~crc_; }

private:
    std::uint32_t crc_ = 0xFFFFFFFFu;
};

void add(Sum& s, const Enemy& e) {
    s.text(e.name.c_str());
    s.u16(e.stats.hp);
    s.byte(e.stats.attack);
    s.byte(e.stats.defense);
    s.f32(e.speed);
    s.byte(static_cast<std::uint8_t>(e.element));
    s.text(data::path(e.sprite)); // references by path: their hashes are the same everywhere anyway
    s.text(data::path(e.hurt_sound));
    s.byte(static_cast<std::uint8_t>(e.drops.size()));
    for (const Drop& d : e.drops) {
        s.text(data::path(d.item));
        s.f32(d.chance);
    }
    s.color(e.tint);
    s.f32(e.spawn.x);
    s.f32(e.spawn.y);
    for (bool flag : e.flags) s.byte(flag ? 1 : 0);
}

void add(Sum& s, const Item& i) {
    s.text(i.name.c_str());
    s.u32(static_cast<std::uint32_t>(i.price));
    s.text(data::path(i.icon));
    s.u16(static_cast<std::uint16_t>(i.heal));
    s.text(data::path(i.summons));
}

void add(Sum& s, const Settings& g) {
    s.text(g.title.c_str());
    s.color(g.top_color);
    s.color(g.bottom_color);
    s.text(data::path(g.music));
    s.text(data::path(g.first));
}

/// Every enemy, every drop followed; "" or what failed.
const char* check_enemies(Sum& sum) {
    for (int i = 0; i < data::count<Enemy>(); ++i) {
        Enemy e;
        if (!data::load(data::ref<Enemy>(i), e)) return "an enemy doesn't load";
        if (data::path(e.sprite)[0] == '\0' || data::path(e.hurt_sound)[0] == '\0') return "a media reference";
        for (const Drop& d : e.drops) {
            Item item;
            if (!data::load(d.item, item)) return "a drop doesn't load";
        }
        add(sum, e);
    }
    return "";
}

/// Every item; the crystal summons the yeti, which drops the crystal (a cycle: followed by hand, once).
const char* check_items(Sum& sum) {
    for (int i = 0; i < data::count<Item>(); ++i) {
        Item item;
        if (!data::load(data::ref<Item>(i), item)) return "an item doesn't load";
        if (!item.summons.empty()) {
            Enemy summoned;
            if (!data::load(item.summons, summoned)) return "a summoned enemy doesn't load";
            bool drops_it = false;
            for (const Drop& d : summoned.drops) drops_it = drops_it || d.item == data::ref<Item>(i);
            if (!drops_it) return "the reference cycle is broken";
        }
        add(sum, item);
    }
    return "";
}

const char* check_settings(Sum& sum) {
    Settings g;
    if (!data::load("game", g)) return "game.settings doesn't load";
    Enemy first;
    if (!data::load(g.first, first) || !(first.name == "Goblin")) return "settings.first isn't the goblin";
    add(sum, g);
    return "";
}

/// Failed loads leave the struct alone and say why.
const char* check_errors() {
    Enemy e;
    e.name.assign("sentinel");
    auto missing = data::load("enemies/nobody", e);
    if (missing || missing.error().code != nnh::ErrorCode::NotFound || !(e.name == "sentinel"))
        return "a missing enemy isn't NotFound";
    Item i;
    i.price = -7;
    auto wrong = data::load("enemies/goblin", i);
    if (wrong || wrong.error().code != nnh::ErrorCode::InvalidArgument || i.price != -7)
        return "an enemy loaded as an item isn't InvalidArgument";
    return "";
}

/// Once the index is loaded, loading, listing and paths allocate nothing (AC-7).
const char* check_no_allocation() {
    const nnh::MemStats before = nnh::mem::region_stats(nnh::MemRegion::Heap);
    const std::size_t index = data::memory_used();
    for (int k = 0; k < 100; ++k) {
        Enemy e;
        (void)data::load(data::ref<Enemy>(k % data::count<Enemy>()), e);
        (void)data::name<Item>(k % data::count<Item>());
        (void)data::path(e.sprite);
    }
    const nnh::MemStats after = nnh::mem::region_stats(nnh::MemRegion::Heap);
    if (after.bytes != before.bytes || after.count != before.count || data::memory_used() != index)
        return "loads allocated memory";
    return "";
}

} // namespace

DataCheckResult run_data_check() {
    DataCheckResult r;
    Sum sum;
    const char* failed = data::preload() ? "" : "the data index doesn't load";
    if (failed[0] == '\0') {
        r.enemies = data::count<Enemy>();
        r.items = data::count<Item>();
        r.settings = data::count<Settings>();
        if (r.enemies != 4 || r.items != 3 || r.settings != 1) failed = "wrong counts";
    }
    for (const char* (*step)(Sum&) : {check_enemies, check_items, check_settings}) {
        if (failed[0] == '\0') failed = step(sum);
    }
    if (failed[0] == '\0') failed = check_errors();
    if (failed[0] == '\0') failed = check_no_allocation();
    std::snprintf(r.failure, sizeof r.failure, "%s", failed);
    r.pass = failed[0] == '\0';
    r.checksum = sum.value();
    r.index_bytes = data::memory_used();
    return r;
}

void format_data_check(const DataCheckResult& r, char* out, std::size_t size) {
    if (r.pass) {
        std::snprintf(out, size, "data check: PASS checksum 0x%08X (%d enemies, %d items, %d settings, index %u bytes)",
                      static_cast<unsigned>(r.checksum), r.enemies, r.items, r.settings,
                      static_cast<unsigned>(r.index_bytes));
    } else {
        std::snprintf(out, size, "data check: FAIL %s", r.failure);
    }
}
