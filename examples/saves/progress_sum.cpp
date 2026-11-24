// A checksum of a Progress's values (progress_sum.hpp).

#include "progress_sum.hpp"

#include <cstring>

namespace saves_example {
namespace {

struct Hash {
    std::uint32_t value = 0x811C9DC5u;

    void byte(std::uint8_t b) {
        value ^= b;
        value *= 0x01000193u;
    }
    void u16(std::uint16_t v) {
        byte(static_cast<std::uint8_t>(v));
        byte(static_cast<std::uint8_t>(v >> 8));
    }
    void u32(std::uint32_t v) {
        u16(static_cast<std::uint16_t>(v));
        u16(static_cast<std::uint16_t>(v >> 16));
    }
    void u64(std::uint64_t v) {
        u32(static_cast<std::uint32_t>(v));
        u32(static_cast<std::uint32_t>(v >> 32));
    }
    void f32(float f) {
        std::uint32_t bits;
        std::memcpy(&bits, &f, 4);
        u32(bits);
    }
    void f64(double d) {
        std::uint64_t bits;
        std::memcpy(&bits, &d, 8);
        u64(bits);
    }
    template <int N> void text(const nnh::data::String<N>& s) {
        const int n = s.size();
        byte(static_cast<std::uint8_t>(n));
        for (int i = 0; i < n; ++i) byte(static_cast<std::uint8_t>(s.c_str()[i]));
    }
    void item(const Item& it) {
        u16(it.kind);
        byte(it.count);
        byte(it.quality);
        u16(it.durability);
        f32(it.wear);
        u32(it.bonus);
        byte(it.dye.r);
        byte(it.dye.g);
        byte(it.dye.b);
        byte(it.dye.a);
        byte(it.slot);
        byte(it.equipped ? 1 : 0);
        text(it.label);
    }
};

} // namespace

std::uint32_t progress_sum(const Progress& p) {
    Hash h;
    h.text(p.name);                                    // 1
    h.u16(p.level);                                    // 2
    h.u64(p.play_frames);                              // 5
    h.f32(p.position.x);                               // 6
    h.f32(p.position.y);
    for (bool e : p.explored) h.byte(e ? 1 : 0);       // 7
    h.byte(p.hero.hp);                                 // 8
    h.byte(p.hero.max_hp);
    h.byte(p.hero.attack);
    h.byte(p.hero.defense);
    h.u16(static_cast<std::uint16_t>(p.inventory.size())); // 9
    for (const Item& it : p.inventory) h.item(it);
    h.u32(p.checkpoint.id);                            // 10
    h.byte(static_cast<std::uint8_t>(p.difficulty));   // 11
    h.u32(p.gems);                                     // 12
    h.u16(p.room);                                     // 13
    h.u16(p.tile);                                     // 14
    h.u64(p.seed);                                     // 15
    h.f64(p.best_time);                                // 17
    for (std::uint16_t c : p.bestiary) h.u16(c);       // 18
    h.u16(static_cast<std::uint16_t>(p.quests.size())); // 19
    for (const Quest& q : p.quests) {
        h.u16(q.id);
        h.byte(static_cast<std::uint8_t>(q.state));
        h.u16(q.progress);
        h.text(q.note);
    }
    return h.value;
}

} // namespace saves_example
