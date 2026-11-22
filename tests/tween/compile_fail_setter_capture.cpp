// Must NOT compile: a setter whose captures are larger than 16 bytes is rejected at compile time with a clear
// message (spec 015 FR-5). Run by ctest: "tween_setter_capture_rejected" expects the message;
// "tween_setter_capture_ok" builds the same file with -DNNH_EXPECT_OK (a small capture) and must compile.
#include "nnh/tween.hpp"

int main() {
    nnh::TweenSet set;
#ifdef NNH_EXPECT_OK
    struct Small {
        char bytes[8];
    } capture{};
#else
    struct Big {
        char bytes[64];
    } capture{};
#endif
    (void)set.from_to(nnh::Setter<float>{[capture](float) { (void)capture; }}, 0.0f, 1.0f, nnh::Frames{10});
    return 0;
}
