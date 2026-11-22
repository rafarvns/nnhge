#include "frame_meter.hpp"

void FrameMeter::add(double ms) {
    ++frames_;
    total_ms_ += ms;
    if (ms > worst_ms_) worst_ms_ = ms;
    if (ms > kMissedMs) ++missed_;
}
