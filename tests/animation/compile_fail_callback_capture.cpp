// Must not compile (spec 016 FR-8): a notice callback whose captures exceed the inline storage (16 bytes). With
// -DNNH_EXPECT_OK the capture fits and it compiles, proving the failure is the size rule.

#include "nnh/animation.hpp"

void install(nnh::SpriteAnimator& animator) {
#if defined(NNH_EXPECT_OK)
    double small[2] = {1, 2};
#else
    double small[3] = {1, 2, 3}; // 24 bytes
#endif
    animator.on_notice([small](nnh::SpriteAnimator&, const nnh::SpriteNotice&) { (void)small; });
}
