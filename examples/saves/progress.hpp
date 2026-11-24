#pragma once
// The saves example's game state, version 3 (spec 027): the reference save of the golden files (AC-2). Declared in a
// header (the "declarations in a header" pattern): every file that includes it can store and load a Progress.
//
//   id  member (v3)    type (v3)                history
//    1  name           String<24>               v1 String<12> (grew in v2)
//    2  level          u16                      v1 u8 (widened in v2)
//    3  -              retired                  v1-v2 coins u32; step 2: gems = coins / 10
//    4  -              retired                  v1 weapon u16; step 1: the first inventory item, equipped
//    5  play_frames    u64                      v1 play_time u32 (renamed and widened in v2)
//    6  position       Vec2
//    7  explored       bool[1024]               the explored-room flags
//    8  hero           Hero                     v1 `stats` without defense (added v2, renamed v3)
//    9  inventory      List<Item, 64>           v1 List<Item, 32> without dye and label (dye v2, label v3)
//   10  checkpoint     data::Ref<Checkpoint>
//   11  difficulty     Difficulty (u8 enum)     v2
//   12  gems           u32                      v3 (step 2)
//   13  room           u16                      v3 (step 2: high half of location)
//   14  tile           u16                      v3 (step 2: low half)
//   15  seed           u64
//   16  -              retired                  v2 location u32 (room << 16 | tile), split by step 2
//   17  best_time      double                   v2
//   18  bestiary       u16[512]                 v2
//   19  quests         List<Quest, 32>          v1 without note (v3)

#include <cstdint>

#include <nnh/gfx.hpp>
#include <nnh/saves.hpp>

namespace saves_example {

enum class Difficulty : std::uint8_t { Easy, Normal, Hard };
enum class QuestState : std::uint8_t { Open, Active, Done };
struct Checkpoint {}; // the data type a checkpoint reference names (the example has no data assets of it)

struct Hero {
    std::uint8_t hp = 10;
    std::uint8_t max_hp = 10;
    std::uint8_t attack = 1;
    std::uint8_t defense = 1;
};
NNH_SAVE_STRUCT(Hero, NNH_SAVE_FIELD(hp, 1), NNH_SAVE_FIELD(max_hp, 2), NNH_SAVE_FIELD(attack, 3),
                NNH_SAVE_FIELD(defense, 4));

struct Item {
    std::uint16_t kind = 0;
    std::uint8_t count = 1;
    std::uint8_t quality = 0;
    std::uint16_t durability = 100;
    float wear = 0;
    std::uint32_t bonus = 0;
    nnh::Color dye{255, 255, 255, 255};
    std::uint8_t slot = 0;
    bool equipped = false;
    nnh::data::String<8> label;
};
NNH_SAVE_STRUCT(Item, NNH_SAVE_FIELD(kind, 1), NNH_SAVE_FIELD(count, 2), NNH_SAVE_FIELD(quality, 3),
                NNH_SAVE_FIELD(durability, 4), NNH_SAVE_FIELD(wear, 5), NNH_SAVE_FIELD(bonus, 6), NNH_SAVE_FIELD(dye, 7),
                NNH_SAVE_FIELD(slot, 8), NNH_SAVE_FIELD(equipped, 9), NNH_SAVE_FIELD(label, 10));

struct Quest {
    std::uint16_t id = 0;
    QuestState state = QuestState::Open;
    std::uint16_t progress = 0;
    nnh::data::String<16> note;
};
NNH_SAVE_STRUCT(Quest, NNH_SAVE_FIELD(id, 1), NNH_SAVE_FIELD(state, 2), NNH_SAVE_FIELD(progress, 3),
                NNH_SAVE_FIELD(note, 4));

inline constexpr int kRooms = 1024; ///< explored flags: a 32 x 32 world of rooms
inline constexpr std::uint32_t kProgressVersion = 3; ///< the save version this game writes

struct Progress {
    nnh::data::String<24> name;
    std::uint16_t level = 1;
    std::uint64_t play_frames = 0;
    nnh::Vec2 position{};
    bool explored[kRooms] = {};
    Hero hero;
    nnh::data::List<Item, 64> inventory;
    nnh::data::Ref<Checkpoint> checkpoint;
    Difficulty difficulty = Difficulty::Normal;
    std::uint32_t gems = 0;
    std::uint16_t room = 0;
    std::uint16_t tile = 0;
    std::uint64_t seed = 0;
    double best_time = 0;
    std::uint16_t bestiary[512] = {};
    nnh::data::List<Quest, 32> quests;
};

// The migration steps (progress_migrate.cpp).
void progress_from_v1(Progress& p, const nnh::save::OldFields& old);
void progress_from_v2(Progress& p, const nnh::save::OldFields& old);

NNH_SAVE_TYPE(Progress, "progress", NNH_SAVE_VERSION(kProgressVersion), NNH_SAVE_STEP(1, progress_from_v1),
              NNH_SAVE_STEP(2, progress_from_v2), NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(level, 2),
              NNH_SAVE_FIELD(play_frames, 5), NNH_SAVE_FIELD(position, 6), NNH_SAVE_FIELD(explored, 7),
              NNH_SAVE_FIELD(hero, 8), NNH_SAVE_FIELD(inventory, 9), NNH_SAVE_FIELD(checkpoint, 10),
              NNH_SAVE_FIELD(difficulty, 11), NNH_SAVE_FIELD(gems, 12), NNH_SAVE_FIELD(room, 13),
              NNH_SAVE_FIELD(tile, 14), NNH_SAVE_FIELD(seed, 15), NNH_SAVE_FIELD(best_time, 17),
              NNH_SAVE_FIELD(bestiary, 18), NNH_SAVE_FIELD(quests, 19), NNH_SAVE_RETIRED(3, 4, 16));

} // namespace saves_example
