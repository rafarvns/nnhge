// Must compile with -Wall -Wextra -Werror (spec 027 T2.3): every kind, shape and declaration form, and every call of
// <nnh/saves.hpp>. The compile_fail cases beside it each break one rule.

#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/saves.hpp"

namespace game {

enum class Element : std::uint8_t { Normal, Fire };
struct Place {};

struct Stats {
    std::uint8_t hp = 10;
    std::uint8_t attack = 1;
};
NNH_SAVE_STRUCT(Stats, NNH_SAVE_FIELD(hp, 1), NNH_SAVE_FIELD(attack, 2));

struct Item {
    std::uint16_t kind = 0;
    nnh::data::String<8> label;
};
NNH_SAVE_STRUCT(Item, NNH_SAVE_FIELD(kind, 1), NNH_SAVE_FIELD(label, 2), NNH_SAVE_RETIRED(3));

struct Progress {
    nnh::data::String<24> name;
    std::uint16_t level = 1;
    std::uint64_t play_frames = 0;
    std::int64_t seed = 0;
    double best_time = 0;
    float speed = 1;
    bool explored[64] = {};
    Stats stats;
    nnh::data::List<Item, 16> inventory;
    nnh::data::List<std::uint8_t, 64> blob;
    nnh::data::Ref<Place> checkpoint;
    nnh::data::TextureRef icon;
    nnh::data::SoundRef sound;
    nnh::data::MusicRef music;
    nnh::Vec2 position{};
    nnh::Color tint{};
    Element element = Element::Normal;
    Item pinned[2];
};
void progress_from_v1(Progress& p, const nnh::save::OldFields& old) {
    std::uint32_t coins = 0;
    if (old.get(nnh::save::path(30), coins)) p.level = static_cast<std::uint16_t>(coins);
    Stats stats;
    (void)old.get(nnh::save::path(8), stats);
    (void)old.has(nnh::save::path(9).item(1).field(2));
}
NNH_SAVE_TYPE(Progress, "progress", NNH_SAVE_VERSION(2), NNH_SAVE_STEP(1, progress_from_v1), NNH_SAVE_FIELD(name, 1),
              NNH_SAVE_FIELD(level, 2), NNH_SAVE_FIELD(play_frames, 5), NNH_SAVE_FIELD(seed, 15),
              NNH_SAVE_FIELD(best_time, 17), NNH_SAVE_FIELD(speed, 18), NNH_SAVE_FIELD(explored, 7),
              NNH_SAVE_FIELD(stats, 8), NNH_SAVE_FIELD(inventory, 9), NNH_SAVE_FIELD(blob, 20),
              NNH_SAVE_FIELD(checkpoint, 10), NNH_SAVE_FIELD(icon, 21), NNH_SAVE_FIELD(sound, 22),
              NNH_SAVE_FIELD(music, 23), NNH_SAVE_FIELD(position, 6), NNH_SAVE_FIELD(tint, 24),
              NNH_SAVE_FIELD(element, 11), NNH_SAVE_FIELD(pinned, 25), NNH_SAVE_RETIRED(3, 4), NNH_SAVE_RETIRED(30));

struct Summary {
    nnh::data::String<24> name;
    std::uint16_t level = 1;
};
NNH_SAVE_SUMMARY(Summary, "progress", 2, NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(level, 2));

nnh::save::Buffer<Progress, 64> g_buffer;

bool use(Progress& p, Summary& s) {
    static_assert(nnh::save::max_size<Progress>() > 0);
    bool ok = nnh::save::store("slot1.sav", p, g_buffer).ok();
    ok = nnh::save::load("slot1.sav", p, g_buffer).ok() && ok;
    ok = nnh::save::load("slot1.sav", s, g_buffer.bytes, sizeof g_buffer.bytes).ok() && ok;
    const auto n = nnh::save::encode(p, g_buffer.bytes, sizeof g_buffer.bytes);
    if (n) ok = nnh::save::decode(g_buffer.bytes, n.value(), p).ok() && ok;
    ok = nnh::save::info("slot1.sav").ok() && nnh::save::exists("slot1.sav") && ok;
    ok = nnh::save::remove("slot2.sav").ok() && ok;
    (void)nnh::save::dump(p, g_buffer.bytes, sizeof g_buffer.bytes, nnh::save::log_lines);
    (void)nnh::save::dump_bytes<Progress>(g_buffer.bytes, 10, nnh::save::log_lines);
    (void)nnh::save::dump_bytes(g_buffer.bytes, 10, nnh::save::log_lines);
    (void)nnh::save::dump_file<Progress>("slot1.sav", g_buffer.bytes, sizeof g_buffer.bytes, nnh::save::log_lines);
    return ok;
}

} // namespace game
