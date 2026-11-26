// The UI example's self-check (spec 028 T7.1, AC-2): 3600 frames of its script on the fake platform give the golden
// checkpoints — the same on every host compiler, the 32-bit host build and the console.

#include <cstdio>
#include <memory>

#include "test.hpp"
#include "ui/self_check.hpp"

NNH_TEST(ui_example_self_check_matches_its_golden) {
    std::unique_ptr<ui_check::SelfCheck> check(new ui_check::SelfCheck());
    CHECK(check->begin());
    int k = 0;
    bool ok = true;
    while (!check->done()) {
        if (!check->step()) continue;
        if (check->checksum() != ui_check::kUiCheckpoints[k]) {
            std::printf("  ui self-check frame %d: %08X (golden %08X)\n", check->frame(),
                        static_cast<unsigned>(check->checksum()), static_cast<unsigned>(ui_check::kUiCheckpoints[k]));
            ok = false;
        }
        ++k;
    }
    CHECK(k == ui_check::kCheckpoints);
    if (check->reached() != 0xFFF) std::printf("  reached %03X\n", static_cast<unsigned>(check->reached()));
    CHECK(check->reached() == 0xFFF); // every screen, the prompt and its Yes, refused, blocked, page, cancel, touch
    CHECK(ok);
    CHECK(check->checksum() == ui_check::kUiCheckGolden);
}
