// Spec 030 T1.1, case "too_deep": a literal name (compile-time check of nnh::live::Name).
#include "nnh/live.hpp"

constexpr nnh::live::Name kName = "a/b/c/d/e";
int main() { return static_cast<int>(kName.length); }
