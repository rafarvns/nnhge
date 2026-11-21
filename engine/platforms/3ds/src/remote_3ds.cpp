// Nintendo 3DS platform: the console remote (contract nnh/platform/remote.hpp). Spec 013 stage 2: not
// available yet — no connection, so core never attaches. Reads and flushed sends on the log socket, and the
// render-target read-back, come in stage 5 (remote_3ds.cpp, remote_capture_3ds.cpp).

#include "context_3ds.hpp"
#include "nnh/platform/remote.hpp"

namespace nnh::platform {

long remote_receive(void*, std::size_t) { return -1; }

bool remote_send(const void*, std::size_t) { return false; }

bool remote_capture(Screen, ScreenPixels&) {
    ctr::set_error("screen read-back is not available yet");
    return false;
}

void remote_capture_release() {}

} // namespace nnh::platform
