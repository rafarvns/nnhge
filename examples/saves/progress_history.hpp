#pragma once
// The saves example's earlier save versions (spec 027): what wrote the golden files v1.sav and v2.sav, and
// the docs' "keep the old struct" pattern — another declaration of the same key with the old ids, which can
// load an old file as it was written. The current game only needs progress.hpp.

#include "progress.hpp"

namespace saves_example {

// ---- Version 1
// -------------------------------------------------------------------------------------------------

struct StatsV1 {
    std::uint8_t hp = 10;
    std::uint8_t max_hp = 10;
    std::uint8_t attack = 1;
};
NNH_SAVE_STRUCT(StatsV1, NNH_SAVE_FIELD(hp, 1), NNH_SAVE_FIELD(max_hp, 2), NNH_SAVE_FIELD(attack, 3));

struct ItemV1 {
    std::uint16_t kind = 0;
    std::uint8_t count = 1;
    std::uint8_t quality = 0;
    std::uint16_t durability = 100;
    float wear = 0;
    std::uint32_t bonus = 0;
    std::uint8_t slot = 0;
    bool equipped = false;
};
NNH_SAVE_STRUCT(ItemV1, NNH_SAVE_FIELD(kind, 1), NNH_SAVE_FIELD(count, 2), NNH_SAVE_FIELD(quality, 3),
                NNH_SAVE_FIELD(durability, 4), NNH_SAVE_FIELD(wear, 5), NNH_SAVE_FIELD(bonus, 6),
                NNH_SAVE_FIELD(slot, 8), NNH_SAVE_FIELD(equipped, 9));

struct QuestV1 {
    std::uint16_t id = 0;
    QuestState state = QuestState::Open;
    std::uint16_t progress = 0;
};
NNH_SAVE_STRUCT(QuestV1, NNH_SAVE_FIELD(id, 1), NNH_SAVE_FIELD(state, 2), NNH_SAVE_FIELD(progress, 3));

struct ProgressV1 {
    nnh::data::String<12> name;
    std::uint8_t level = 1;
    std::uint32_t coins = 0;
    std::uint16_t weapon = 0;
    std::uint32_t play_time = 0;
    nnh::Vec2 position{};
    bool explored[kRooms] = {};
    StatsV1 stats;
    nnh::data::List<ItemV1, 32> inventory;
    nnh::data::Ref<Checkpoint> checkpoint;
    std::uint64_t seed = 0;
    nnh::data::List<QuestV1, 32> quests;
};
NNH_SAVE_TYPE(ProgressV1, "progress", NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(level, 2),
              NNH_SAVE_FIELD(coins, 3), NNH_SAVE_FIELD(weapon, 4), NNH_SAVE_FIELD(play_time, 5),
              NNH_SAVE_FIELD(position, 6), NNH_SAVE_FIELD(explored, 7), NNH_SAVE_FIELD(stats, 8),
              NNH_SAVE_FIELD(inventory, 9), NNH_SAVE_FIELD(checkpoint, 10), NNH_SAVE_FIELD(seed, 15),
              NNH_SAVE_FIELD(quests, 19));

// ---- Version 2
// -------------------------------------------------------------------------------------------------

struct StatsV2 {
    std::uint8_t hp = 10;
    std::uint8_t max_hp = 10;
    std::uint8_t attack = 1;
    std::uint8_t defense = 1;
};
NNH_SAVE_STRUCT(StatsV2, NNH_SAVE_FIELD(hp, 1), NNH_SAVE_FIELD(max_hp, 2), NNH_SAVE_FIELD(attack, 3),
                NNH_SAVE_FIELD(defense, 4));

struct ItemV2 {
    std::uint16_t kind = 0;
    std::uint8_t count = 1;
    std::uint8_t quality = 0;
    std::uint16_t durability = 100;
    float wear = 0;
    std::uint32_t bonus = 0;
    nnh::Color dye{255, 255, 255, 255};
    std::uint8_t slot = 0;
    bool equipped = false;
};
NNH_SAVE_STRUCT(ItemV2, NNH_SAVE_FIELD(kind, 1), NNH_SAVE_FIELD(count, 2), NNH_SAVE_FIELD(quality, 3),
                NNH_SAVE_FIELD(durability, 4), NNH_SAVE_FIELD(wear, 5), NNH_SAVE_FIELD(bonus, 6),
                NNH_SAVE_FIELD(dye, 7), NNH_SAVE_FIELD(slot, 8), NNH_SAVE_FIELD(equipped, 9));

struct ProgressV2 {
    nnh::data::String<24> name;
    std::uint16_t level = 1;
    std::uint32_t coins = 0;
    std::uint64_t play_frames = 0;
    nnh::Vec2 position{};
    bool explored[kRooms] = {};
    StatsV2 stats;
    nnh::data::List<ItemV2, 64> inventory;
    nnh::data::Ref<Checkpoint> checkpoint;
    Difficulty difficulty = Difficulty::Normal;
    std::uint64_t seed = 0;
    std::uint32_t location = 0;
    double best_time = 0;
    std::uint16_t bestiary[512] = {};
    nnh::data::List<QuestV1, 32> quests;
};
void progress_v2_from_v1(ProgressV2& p, const nnh::save::OldFields& old); // progress_migrate.cpp
NNH_SAVE_TYPE(ProgressV2, "progress", NNH_SAVE_VERSION(2), NNH_SAVE_STEP(1, progress_v2_from_v1),
              NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(level, 2), NNH_SAVE_FIELD(coins, 3),
              NNH_SAVE_FIELD(play_frames, 5), NNH_SAVE_FIELD(position, 6), NNH_SAVE_FIELD(explored, 7),
              NNH_SAVE_FIELD(stats, 8), NNH_SAVE_FIELD(inventory, 9), NNH_SAVE_FIELD(checkpoint, 10),
              NNH_SAVE_FIELD(difficulty, 11), NNH_SAVE_FIELD(seed, 15), NNH_SAVE_FIELD(location, 16),
              NNH_SAVE_FIELD(best_time, 17), NNH_SAVE_FIELD(bestiary, 18), NNH_SAVE_FIELD(quests, 19),
              NNH_SAVE_RETIRED(4));

} // namespace saves_example
