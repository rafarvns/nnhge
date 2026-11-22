#include "self_check.hpp"

#include <cstdio>
#include <cstring>

namespace {
bool is(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
} // namespace

void ActionsSelfCheck::remap(const char* action, nnh::Source source, nnh::Resolution resolution, const char* other) {
    if (!is(action, "jump")) return;
    if (source == nnh::Source::button(nnh::Button::B) && resolution == nnh::Resolution::Swapped && is(other, "attack")) {
        steps_ |= kSwap;
    } else if (source == nnh::Source::button(nnh::Button::Y) && resolution == nnh::Resolution::None &&
               (steps_ & kSwap) != 0) {
        steps_ |= kReboundY;
    }
}

void ActionsSelfCheck::saved(std::uint32_t sum) {
    if ((steps_ & kReboundY) == 0) return;
    steps_ |= kSaved;
    saved_sum_ = sum;
}

void ActionsSelfCheck::pressed(const char* action) {
    if ((steps_ & kSaved) == 0) return;
    if (is(action, "jump")) steps_ |= kJump;
    if (is(action, "attack")) steps_ |= kAttack;
}

void ActionsSelfCheck::reloaded(std::uint32_t sum, int applied, int skipped) {
    if ((steps_ & kSaved) != 0 && sum == saved_sum_ && applied > 0 && skipped == 0) steps_ |= kReload;
}

void ActionsSelfCheck::bench(float avg_ms, bool console) {
    done_ = true;
    if (avg_ms >= 0.0f && (!console || avg_ms < 0.1f)) steps_ |= kBench;
}

void ActionsSelfCheck::verdict(char* out, std::size_t size) const {
    struct Step {
        std::uint8_t bit;
        const char* missing;
    };
    static constexpr Step kSteps[] = {{kSwap, "no swap"},
                                      {kReboundY, "no rebind to Y"},
                                      {kSaved, "not saved"},
                                      {kJump, "no jump on the new button"},
                                      {kAttack, "no attack on A"},
                                      {kReload, "reload differs"},
                                      {kBench, "bench over 0.1 ms or not measured"}};
    for (const Step& s : kSteps) {
        if ((steps_ & s.bit) == 0) {
            std::snprintf(out, size, "actions: FAIL %s", s.missing);
            return;
        }
    }
    std::snprintf(out, size, "actions: PASS 7/7");
}
