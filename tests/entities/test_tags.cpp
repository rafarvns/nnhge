// Tags (spec 020 FR-16, FR-17; T4.1, T4.2): compile-time and run-time hashes equal (FNV-1a vectors), case labels,
// tags_distinct; add_tag/remove_tag/has_tag, nnh::kNoTag, the per-entity limit, tags gone after destroy, 0 tags.

#include "nnh/detail/name_hash.hpp"
#include "world_fixture.hpp"

using nnh::Entity;
using nnh::Tag;
using nnh::World;

namespace {

constexpr Tag kEnemy = nnh::tag("enemy");
constexpr Tag kCoin = nnh::tag("coin");
constexpr Tag kFlying = nnh::tag("flying");
static_assert(nnh::tags_distinct(kEnemy, kCoin, kFlying));
static_assert(nnh::tag("a") == Tag{0xE40C292Cu}); // the published FNV-1a 32 vector
static_assert(nnh::tag("") == Tag{0x811C9DC5u});

const char* name_of(Tag t) {
    switch (t) {
    case nnh::tag("enemy"):
        return "enemy";
    case kCoin:
        return "coin";
    default:
        return "?";
    }
}

} // namespace

NNH_TEST(tag_hashes_at_run_time_equal_the_compile_time_ones) {
    const char buffer[] = {'f', 'l', 'y', 'i', 'n', 'g', '!', '!'}; // not NUL-terminated
    CHECK(nnh::tag(buffer, 6) == kFlying);
    CHECK(nnh::tag("foobar") == Tag{0xBF9CF968u});
    char name[8] = "coin";
    CHECK(nnh::tag(name) == kCoin);
    CHECK(static_cast<std::uint32_t>(kEnemy) == nnh::detail::fnv1a32("enemy"));
}

NNH_TEST(tags_work_as_case_labels) {
    CHECK(name_of(nnh::tag("enemy"))[0] == 'e');
    CHECK(name_of(kCoin)[0] == 'c');
    CHECK(name_of(kFlying)[0] == '?');
}

NNH_TEST(tags_distinct_refuses_duplicates_and_the_none_tag) {
    CHECK(nnh::tags_distinct(kEnemy, kCoin));
    CHECK(!nnh::tags_distinct(kEnemy, kCoin, kEnemy));
    CHECK(!nnh::tags_distinct(kEnemy, nnh::kNoTag));
    CHECK(nnh::tags_distinct());
    CHECK(nnh::tags_distinct(kCoin));
}

NNH_TEST(add_remove_and_has_tag) {
    World w = world_test::make_world();
    const Entity e = w.spawn();
    CHECK(w.add_tag(e, kEnemy));
    CHECK(!w.add_tag(e, kEnemy)); // already there
    CHECK(w.add_tag(e, kFlying));
    CHECK(w.has_tag(e, kEnemy) && w.has_tag(e, kFlying) && !w.has_tag(e, kCoin));
    CHECK(w.remove_tag(e, kEnemy));
    CHECK(!w.remove_tag(e, kEnemy));
    CHECK(!w.has_tag(e, kEnemy) && w.has_tag(e, kFlying));
    CHECK(w.add_tag(e, kCoin)); // the freed word is reused
    CHECK(!w.has_tag(e, nnh::kNoTag));
}

NNH_TEST(tags_of_stale_entities_answer_false) {
    World w = world_test::make_world();
    const Entity e = w.spawn();
    w.add_tag(e, kEnemy);
    w.destroy(e);
    CHECK(!w.has_tag(e, kEnemy));
    CHECK(!w.add_tag(e, kCoin) && !w.remove_tag(e, kEnemy));
    w.flush();
    const Entity again = w.spawn();
    CHECK(again.slot() == e.slot() && !w.has_tag(again, kEnemy)); // tags went with the entity
}

namespace {

World* g_w = nullptr;
Entity g_e{};
void add_a_fifth_tag() { CHECK(!g_w->add_tag(g_e, nnh::tag("fifth"))); }
void add_the_none_tag() { CHECK(!g_w->add_tag(g_e, nnh::kNoTag)); }
void add_a_tag_without_tag_words() { CHECK(!g_w->add_tag(g_e, kEnemy)); }

} // namespace

NNH_TEST(the_per_entity_limit_and_the_none_tag_assert) {
    World w = world_test::make_world(16, 16, 4);
    g_w = &w;
    g_e = w.spawn();
    for (const char* n : {"a", "b", "c", "d"}) CHECK(w.add_tag(g_e, nnh::tag(n)));
    CHECK(world_test::asserts_or_is_ignored(add_a_fifth_tag));
    CHECK(world_test::asserts_or_is_ignored(add_the_none_tag));
    CHECK(!w.has_tag(g_e, nnh::tag("fifth")));
}

NNH_TEST(a_world_without_tags_refuses_every_tag) {
    World w = world_test::make_world(16, 16, 0);
    g_w = &w;
    g_e = w.spawn();
    CHECK(world_test::asserts_or_is_ignored(add_a_tag_without_tag_words));
    CHECK(w.find(kEnemy, nullptr, 0) == 0 && !w.first(kEnemy));
}
