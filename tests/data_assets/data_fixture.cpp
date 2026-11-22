// Shared by the loading tests (data_fixture.hpp).

#include "data_fixture.hpp"

#include <algorithm>
#include <string>

#include "data/schema_fingerprint.hpp"
#include "nnh/app.hpp"
#include "nnh/detail/name_hash.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"
#include "test_types.hpp"

namespace data_test {

namespace {

namespace fake = nnh::platform_fake;
using data_file::Writer;
using nnh::detail::fnv1a32;

std::uint32_t fingerprint_of(nnh::detail::TypeId tag) {
    const auto& registry = nnh::detail::data_registry();
    return nnh::detail::schema_fingerprint(registry, *nnh::detail::find_type(registry, tag));
}

data_file::Bytes goblin() {
    return Writer(enemy_fingerprint())
        .fields(12)
        .field(0)
        .str("Goblin")
        .field(1)
        .fields(2)
        .field(0)
        .u16(30)
        .field(1)
        .u8(0xFD) // stats {hp 30, attack -3}
        .field(2)
        .f32(1.5f)
        .field(3)
        .u8(1) // Fire
        .field(4)
        .u32(fnv1a32("art/goblin"))
        .field(5)
        .u32(fnv1a32("sfx/hurt"))
        .field(6)
        .u16(1)
        .fields(2)
        .field(0)
        .u32(fnv1a32("items/potion"))
        .field(1)
        .f32(0.25f)
        .field(7)
        .u8(10)
        .u8(20)
        .u8(30)
        .u8(40)
        .field(8)
        .f32(3)
        .f32(-4)
        .field(9)
        .u8(1)
        .u8(0)
        .u8(1)
        .field(10)
        .u8(7)
        .field(11)
        .u32(fnv1a32("music/battle"))
        .bytes();
}

struct Live {
    Files files;
};

Live& live() {
    static Live l;
    return l;
}

} // namespace

std::uint32_t enemy_fingerprint() {
    return fingerprint_of(nnh::detail::type_id<test_data::Enemy>());
}
std::uint32_t item_fingerprint() {
    return fingerprint_of(nnh::detail::type_id<test_data::Item>());
}

data_file::IndexSpec standard_index() {
    data_file::IndexSpec s;
    s.types = {{"enemy", enemy_fingerprint(), {"enemies/bat", "enemies/goblin"}},
               {"item", item_fingerprint(), {"items/ether", "items/potion"}}};
    s.media[0] = {"art/goblin", "art/potion"};
    s.media[1] = {"sfx/hurt"};
    s.media[2] = {"music/battle"};
    return s;
}

Files standard() {
    Files f;
    f.bat = Writer(enemy_fingerprint()).fields(1).field(0).str("Bat").bytes();
    f.goblin = goblin();
    f.ether = Writer(item_fingerprint()).fields(2).field(0).str("Ether").field(1).u32(50).bytes();
    f.potion = Writer(item_fingerprint())
                   .fields(3)
                   .field(0)
                   .str("Potion")
                   .field(1)
                   .u32(5)
                   .field(2)
                   .u32(fnv1a32("art/potion"))
                   .bytes();
    data_file::IndexSpec spec = standard_index();
    spec.largest = static_cast<std::uint32_t>(
        std::max({f.bat.size(), f.goblin.size(), f.ether.size(), f.potion.size()}));
    f.index = data_file::index_bytes(spec);
    return f;
}

void start(const Files& files) {
    stop();
    fake::reset();
    live().files = files;
    const Files& f = live().files;
    static const char kAssets[] =
        ".nnh_data\nenemies/bat.enemy\nenemies/e398497.enemy\nenemies/goblin.enemy\nitems/ether.item\n"
        "items/potion.item\n";
    fake::add_asset(".nnh_assets", kAssets, sizeof kAssets - 1);
    fake::add_asset(".nnh_data", f.index.data(), f.index.size());
    fake::add_asset("enemies/bat.enemy", f.bat.data(), f.bat.size());
    fake::add_asset("enemies/goblin.enemy", f.goblin.data(), f.goblin.size());
    // A file whose name shares its hash with "enemies/e1059030" (only some tests' indexes list it).
    fake::add_asset("enemies/e398497.enemy", f.bat.data(), f.bat.size());
    fake::add_asset("items/ether.item", f.ether.data(), f.ether.size());
    fake::add_asset("items/potion.item", f.potion.data(), f.potion.size());
    CHECK(nnh::init());
}

void stop() {
    nnh::data::release(); // Release builds don't run shutdown hooks
    nnh::shutdown();
}

} // namespace data_test
