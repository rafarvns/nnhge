// Spec 030 T1.1, case "too_long": a literal name (compile-time check of nnh::live::Name).
#include "nnh/live.hpp"

constexpr nnh::live::Name kName = "abcdefghijklmnopqrstuvwxyz012345";
int main() { return static_cast<int>(kName.length); }
