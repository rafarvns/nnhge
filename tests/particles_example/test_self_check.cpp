// The particles example's self-check (spec 024 AC-2; T5.1): 8 emitters, 3600 frames of a fixed script, a checksum of
// every live particle every 600 frames against the goldens. The same values on every host compiler and on the 3DS.

#include <cstdio>

#include "particles/self_check.hpp"
#include "test.hpp"

NNH_TEST(particles_self_check_matches_its_goldens) {
    particles_check::SelfCheck check;
    CHECK(check.begin());
    int checkpoint = 0;
    int max_alive = 0;
    while (!check.done()) {
        if (check.step()) {
            std::printf("  particles checkpoint %d: %08X\n", check.frame(), static_cast<unsigned>(check.checksum()));
            CHECK(check.checksum() == particles_check::kParticlesCheckpoints[checkpoint]);
            ++checkpoint;
        }
        max_alive = check.total_alive() > max_alive ? check.total_alive() : max_alive;
    }
    CHECK(checkpoint == particles_check::kCheckpoints);
    CHECK(check.checksum() == particles_check::kParticlesCheckGolden);
    CHECK(check.emitter(1).stats().dropped > 0); // E2 was kept full
    CHECK(check.emitter(7).finished()); // E8: restarted at 2400, its 600 frames over by 3060
    std::printf("  particles self-check %08X, max live %d, E2 dropped %u\n",
                static_cast<unsigned>(check.checksum()), max_alive,
                static_cast<unsigned>(check.emitter(1).stats().dropped));
}
