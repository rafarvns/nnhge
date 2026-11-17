// Must NOT compile for PC: 3DS extension headers stop non-3DS builds with a clear #error.
// Run by ctest (test "extension_header_rejected_off_platform", WILL_FAIL).
#include "nnh/3ds/console.hpp"

int main() { return 0; }
