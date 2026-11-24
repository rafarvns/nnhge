// The reference values of the saves example (reference.hpp). Integer arithmetic only (floats are made from
// their bits), so every compiler and the 3DS build the same values.

#include "reference.hpp"

#include <cstring>
#include <new>
#include <nnh/detail/name_hash.hpp>

namespace saves_example {
namespace {

/// A seeded FNV-1a sequence: each value is the hash of the previous one.
struct Sequence {
    std::uint32_t state;
    std::uint32_t next() {
        std::uint8_t bytes[4];
        std::memcpy(bytes, &state, 4);
        state = nnh::detail::fnv1a32(reinterpret_cast<const char*>(bytes), 4);
        return state;
    }
};

float float_bits(std::uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

constexpr const char* kName = "Zo\xC3\xAB Rangers"; // "Zoë Rangers": 12 bytes, one two-byte character
constexpr std::uint32_t kCoins = 123456;
constexpr std::uint16_t kWeapon = 501;
constexpr std::uint16_t kRoom = 12;
constexpr std::uint16_t kTile = 345;
constexpr std::uint64_t kSeed = 0x1234567890ull;     // above 2^32
constexpr std::uint64_t kPlayFrames = 5000000000ull; // above 2^32 (v2, v3)
constexpr std::uint32_t kPlayTime = 3600000;         // v1's u32
constexpr double kBestTime = 123.456;

/// The parts every version shares, from one sequence (`items` items, `quests` quests).
template <class P, class ItemT, class QuestT> void fill_common(P& p, Sequence& seq, int items) {
    p.name.assign(kName);
    p.position = {123.25f, -45.5f};
    for (int i = 0; i < kRooms; ++i)
        p.explored[i] = (seq.next() & 3u) == 0;
    for (int i = 0; i < items; ++i) {
        ItemT it{};
        it.kind = static_cast<std::uint16_t>(100 + i);
        it.count = static_cast<std::uint8_t>(1 + seq.next() % 20);
        it.quality = static_cast<std::uint8_t>(seq.next() % 5);
        it.durability = static_cast<std::uint16_t>(seq.next() % 1000);
        it.wear =
            float_bits(i == 5 ? 0x00000123u : 0x3F000000u | (seq.next() & 0x7FFFFFu)); // item 5: subnormal
        it.bonus = seq.next();
        it.slot = static_cast<std::uint8_t>(i % 8);
        it.equipped = i == 3;
        p.inventory.push_back(it);
    }
    p.checkpoint.id = nnh::detail::fnv1a32("checkpoints/forest");
    p.seed = kSeed;
    for (int i = 0; i < 20; ++i) {
        QuestT q{};
        q.id = static_cast<std::uint16_t>(1000 + i);
        q.state = static_cast<QuestState>(i % 3);
        q.progress = static_cast<std::uint16_t>(seq.next() % 100);
        p.quests.push_back(q);
    }
}

template <class P> void fill_v2_extras(P& p, Sequence& seq) {
    for (std::uint16_t& count : p.bestiary)
        count = static_cast<std::uint16_t>(seq.next() % 60000);
    for (auto& it : p.inventory) {
        const std::uint32_t c = seq.next();
        it.dye = {static_cast<std::uint8_t>(c), static_cast<std::uint8_t>(c >> 8),
                  static_cast<std::uint8_t>(c >> 16), 255};
    }
    p.difficulty = Difficulty::Hard;
    p.best_time = kBestTime;
    p.play_frames = kPlayFrames;
    p.level = 300;
}

void fill_v3_extras(Progress& p) {
    char text[16];
    for (int i = 0; i < p.inventory.size(); ++i) {
        if (i % 4 != 0) continue;
        text[0] = 'i';
        text[1] = 't';
        text[2] = static_cast<char>('0' + i / 10);
        text[3] = static_cast<char>('0' + i % 10);
        text[4] = '\0';
        p.inventory[i].label.assign(text);
    }
    for (int i = 0; i < p.quests.size(); ++i) {
        if (i % 2 == 0) p.quests[i].note.assign(i == 0 ? "find the \"key\"" : "q");
    }
    p.gems = kCoins / 10;
    p.room = kRoom;
    p.tile = kTile;
    p.hero = {40, 50, 7, 3};
}

} // namespace

void fill_reference_v1(ProgressV1& p) {
    ::new (&p) ProgressV1{};
    Sequence seq{1};
    fill_common<ProgressV1, ItemV1, QuestV1>(p, seq, 32);
    p.level = 37;
    p.coins = kCoins;
    p.weapon = kWeapon;
    p.play_time = kPlayTime;
    p.stats = {40, 50, 7};
}

void fill_reference_v2(ProgressV2& p) {
    ::new (&p) ProgressV2{};
    Sequence seq{1};
    fill_common<ProgressV2, ItemV2, QuestV1>(p, seq, 64);
    fill_v2_extras(p, seq);
    p.coins = kCoins;
    p.location = static_cast<std::uint32_t>(kRoom) << 16 | kTile;
    p.stats = {40, 50, 7, 3};
}

void fill_reference_v3(Progress& p) {
    ::new (&p) Progress{};
    Sequence seq{1};
    fill_common<Progress, Item, Quest>(p, seq, 64);
    fill_v2_extras(p, seq);
    fill_v3_extras(p);
}

void fill_reference_v1_migrated(Progress& p) {
    static ProgressV1 v1; // static: no 3 KB temporary on the stack
    fill_reference_v1(v1);
    ::new (&p) Progress{}; // T{}: what the fields the v1 file lacks keep
    p.name.assign(v1.name.c_str());
    p.level = v1.level;
    p.play_frames = v1.play_time;
    p.position = v1.position;
    for (int i = 0; i < kRooms; ++i)
        p.explored[i] = v1.explored[i];
    p.hero.hp = v1.stats.hp;
    p.hero.max_hp = v1.stats.max_hp;
    p.hero.attack = v1.stats.attack;
    Item weapon{}; // step 1: the weapon, first and equipped
    weapon.kind = v1.weapon;
    weapon.equipped = true;
    p.inventory.push_back(weapon);
    for (const ItemV1& old : v1.inventory) {
        Item it{};
        it.kind = old.kind;
        it.count = old.count;
        it.quality = old.quality;
        it.durability = old.durability;
        it.wear = old.wear;
        it.bonus = old.bonus;
        it.slot = old.slot;
        it.equipped = old.equipped;
        p.inventory.push_back(it);
    }
    p.checkpoint = v1.checkpoint;
    p.seed = v1.seed;
    for (const QuestV1& old : v1.quests)
        p.quests.push_back({old.id, old.state, old.progress, {}});
    p.gems = v1.coins / 10; // step 2 (no location in v1: room and tile keep their defaults)
}

void fill_reference_v3_b(Progress& p) {
    fill_reference_v3(p);
    p.name.assign("Second Slot");
    p.level = 301;
    p.gems += 1;
    p.inventory.clear();
    p.explored[0] = !p.explored[0];
}

} // namespace saves_example
