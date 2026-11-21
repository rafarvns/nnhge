// PC simulator: the console remote (contract nnh/platform/remote.hpp). Spec 013 stage 2: not available yet —
// no connection, so core never attaches. The loopback socket from NNH_REMOTE and the screen read-back come in
// stage 4 (remote_pc.cpp, remote_capture_pc.cpp).

#include "context_pc.hpp"
#include "nnh/platform/remote.hpp"

namespace nnh::platform {

long remote_receive(void*, std::size_t) { return -1; }

bool remote_send(const void*, std::size_t) { return false; }

bool remote_capture(Screen, ScreenPixels&) {
    pc::set_error("screen read-back is not available yet");
    return false;
}

void remote_capture_release() {}

} // namespace nnh::platform
