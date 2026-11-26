// Spec 030 T1.1, case "empty_part": a literal name (compile-time check of nnh::live::Name).
#include "nnh/live.hpp"

constexpr nnh::live::Name kName = "fx//spark";
int main() { return static_cast<int>(kName.length); }
