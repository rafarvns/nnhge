// Must NOT compile for PC: <nnh/3ds/network.hpp> stops non-3DS builds with a clear #error.
// Run by ctest (test "extension_network_rejected_off_platform", WILL_FAIL).
#include "nnh/3ds/network.hpp"

int main() { return 0; }
