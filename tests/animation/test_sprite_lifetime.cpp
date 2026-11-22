// The library lifetime rule (T3.1; FR-5): an animator used after its library is destroyed asserts in
// Debug/Development (Release doesn't check, so the test is skipped there); serial slots are reused.

#include <utility>

#include "library_fixture.hpp"

using nnh::SpriteAnimator;
using nnh::SpriteLibrary;

namespace {

SpriteAnimator* g_orphan = nullptr;

} // namespace

NNH_TEST(using_an_animator_after_its_library_is_destroyed_asserts) {
    if (!nnh::build::kAssertsEnabled) return;
    SpriteAnimator orphan;
    {
        SpriteLibrary lib = anim_test::strip(4);
        orphan = SpriteAnimator{lib};
        orphan.play(anim_test::clip(lib, "c", 4, 1));
    }
    g_orphan = &orphan;
    CHECK(anim_test::crashes([] { g_orphan->update(); }));
    CHECK(anim_test::crashes([] { g_orphan->play("c"); }));
    CHECK(anim_test::crashes([] { (void)g_orphan->frame(); }));
}

NNH_TEST(a_library_moved_into_another_variable_stays_alive_for_its_animators) {
    SpriteLibrary first = anim_test::strip(4);
    SpriteAnimator a{first};
    a.play(anim_test::clip(first, "c", 4, 1));
    SpriteLibrary second = std::move(first);
    a.update();
    CHECK(a.position() == 1);
}

NNH_TEST(many_libraries_come_and_go_without_running_out_of_serials) {
    for (int i = 0; i < 3000; ++i) {
        SpriteLibrary lib = anim_test::strip(1);
        SpriteAnimator a{lib};
        a.play(anim_test::clip(lib, "c", 1, 1));
        a.update();
        CHECK(a.frame() == 0);
    }
}
