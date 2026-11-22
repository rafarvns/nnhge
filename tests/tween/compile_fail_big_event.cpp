// Must NOT compile: an event published on finish larger than 32 bytes is rejected at compile time with a
// clear message (spec 015 FR-11). Run by ctest: "tween_big_event_rejected" expects the message;
// "tween_big_event_ok" builds the same file with -DNNH_EXPECT_OK (a 32-byte event) and must compile.
#include "nnh/tween.hpp"

struct Finished {
#ifdef NNH_EXPECT_OK
    char bytes[32];
#else
    char bytes[33];
#endif
};

int main() {
    nnh::TweenSet set;
    float x = 0;
    (void)set.to(&x, 1.0f, nnh::Frames{10}, {.publish = Finished{}});
    (void)set.after(nnh::Frames{10}, Finished{});
    return 0;
}
