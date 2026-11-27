#include "door_run.hpp"

#include <cstdio>
#include <nnh/build.hpp>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(platformer);

namespace {

const char* demo_name(Demo demo) { return demo == Demo::Bonus ? "bonus" : "level1"; }

} // namespace

void DoorRun::start() {
    *this = {};
    running_ = true;
    std::snprintf(line_, sizeof line_, "doors: started");
    log();
}

PlayerInput DoorRun::input() const { return {.run = 1}; }

void DoorRun::frame(double ms) {
    if (!running_) return;
    ++frames_;
    if (frames_ > 1) meter_.add(ms); // the first frame's time includes whatever came before the run
}

void DoorRun::door(Demo from, Demo to) {
    if (!running_) return;
    ++doors_;
    std::snprintf(line_, sizeof line_, "doors: %d %s -> %s frame %d", doors_, demo_name(from), demo_name(to),
                  frames_);
    log();
}

void DoorRun::door_done() {
    if (!running_ || ++finished_ < kDoors) return;
    running_ = false;
    // 60 Hz is a console budget: judged in Development builds (3ds-dev on the console, pc-dev), like the other benches;
    // a Debug build (the ctest runs pc-debug, often next to other tests) only reports its times.
    const bool judged = nnh::build::flavor() == nnh::build::Flavor::Development;
    std::snprintf(line_, sizeof line_, "doors: done %d irises frames %d worst %.1f ms avg %.1f ms: 60 Hz %s", kDoors,
                  frames_, meter_.worst_ms(), meter_.average_ms(),
                  !judged ? "not judged (Debug)" : meter_.ok() ? "PASS" : "FAIL");
    log();
}

void DoorRun::log() { NNH_LOG_INFO(platformer, "%s", line_); }
