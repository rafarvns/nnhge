// Must NOT compile: a handler capture larger than the small-function capacity is rejected at compile
// time with a clear message. Run by ctest (test "large_handler_capture_rejected", WILL_FAIL).
#include "nnh/events.hpp"

struct Ping {
    int value;
};

int main() {
    struct Big {
        char bytes[64];
    } big{};
    nnh::Subscription sub = nnh::events::subscribe<Ping>([big](const Ping&) { (void)big; });
    return 0;
}
