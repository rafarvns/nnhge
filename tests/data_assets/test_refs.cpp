// References of data assets (spec 014 FR-8): small values (a 32-bit path hash, 0 = none), comparable,
// testable for emptiness, trivially copyable. Following them and turning them into paths come with the index
// (stage 3).

#include <type_traits>

#include "nnh/data.hpp"
#include "test.hpp"

namespace {
struct Item {
    int price = 0;
};
} // namespace

using nnh::data::MusicRef;
using nnh::data::Ref;
using nnh::data::SoundRef;
using nnh::data::TextureRef;

static_assert(sizeof(Ref<Item>) == 4 && std::is_trivially_copyable_v<Ref<Item>>);
static_assert(sizeof(TextureRef) == 4 && sizeof(SoundRef) == 4 && sizeof(MusicRef) == 4);
static_assert(std::is_standard_layout_v<TextureRef> && std::is_trivially_copyable_v<MusicRef>);
// A texture reference is not a sound reference: no accidental mixing.
static_assert(!std::is_convertible_v<TextureRef, SoundRef> && !std::is_convertible_v<SoundRef, MusicRef>);

NNH_TEST(data_refs_start_empty) {
    CHECK(Ref<Item>{}.empty());
    CHECK(TextureRef{}.empty());
    CHECK(SoundRef{}.empty());
    CHECK(MusicRef{}.empty());
}

NNH_TEST(data_refs_compare_by_id) {
    const Ref<Item> a{0x1234u};
    const Ref<Item> b{0x1234u};
    const Ref<Item> c{0x9999u};
    CHECK(a == b);
    CHECK(!(a == c));
    CHECK(!a.empty());
    CHECK(TextureRef{5} == TextureRef{5});
    CHECK(!(SoundRef{5} == SoundRef{6}));
    CHECK(MusicRef{1} == MusicRef{1});
}
