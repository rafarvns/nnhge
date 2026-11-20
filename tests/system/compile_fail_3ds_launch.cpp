// Must NOT compile for PC: <nnh/3ds/launch.hpp> stops non-3DS builds with a clear #error.
// Run by ctest (test "extension_launch_rejected_off_platform", WILL_FAIL).
#include "nnh/3ds/launch.hpp"

int main() { return 0; }
