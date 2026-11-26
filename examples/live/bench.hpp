#pragma once
// The example's bench (spec 030 AC-7; X on the showcase or LIVE_DEMO=bench): 300 frames per phase, each timing only
// its workload, then `live bench: <phase> avg X ms worst Y ms (target < Z ms) PASS|FAIL` lines and the verdict.
//   show64      64 show() calls a frame (timed around the calls)
//   hidden192   128 tunables + 64 watches registered, overlay hidden: the application point and the frame end
//               (profile scopes "live start" + "live end", Development builds)
//   tunables    the Tunables page shown: its CPU cost a frame (profile scope "live page")
//   watches     the Watches page shown (17 rows)
//   register    registering and removing 100 tunables and 50 watches (once per frame)
// The profile scopes exist in Development builds only: Debug logs `not judged (Debug)`.

#include <cstdint>

#include <nnh/live.hpp>

class LiveBench {
public:
    void start();
    /// Once per frame from the scene's update while running. @return false when done.
    bool update();
    bool running() const { return phase_ >= 0; }

private:
    void begin_phase();
    void end_phase();
    void report();

    int phase_ = -1;
    int frame_ = 0;
    double sums_[5] = {}, worsts_[5] = {};
    float values_[118] = {};
    int ints_[60] = {};
    nnh::live::Handle tunables_[118];
    nnh::live::Handle watches_[60];
};
