// Spec 030 T1.1, case "good": a literal name (compile-time check of nnh::live::Name).
#include "nnh/live.hpp"

constexpr nnh::live::Name kName = "player/jump";
int main() { return static_cast<int>(kName.length); }
