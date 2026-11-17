// Must NOT compile: a handler capture larger than the small-function capacity is rejected at compile
// time with a clear message. Run by ctest (test "large_handler_capture_rejected", WILL_FAIL).
#include "nnh/detail/small_function.hpp"

int main() {
    struct Big {
        char bytes[64];
    } big{};
    nnh::detail::SmallFunction<void()> f = [big]() { (void)big; };
    f();
    return 0;
}
