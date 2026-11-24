// Must not compile: nine struct levels (spec 027 T2.3, tests/CMakeLists.txt checks the message).

#include <cstdint>

#include "nnh/saves.hpp"

struct L1 {
    std::uint8_t a = 0;
};
NNH_SAVE_STRUCT(L1, NNH_SAVE_FIELD(a, 1));
struct L2 {
    L1 inner;
};
NNH_SAVE_STRUCT(L2, NNH_SAVE_FIELD(inner, 1));
struct L3 {
    L2 inner;
};
NNH_SAVE_STRUCT(L3, NNH_SAVE_FIELD(inner, 1));
struct L4 {
    L3 inner;
};
NNH_SAVE_STRUCT(L4, NNH_SAVE_FIELD(inner, 1));
struct L5 {
    L4 inner;
};
NNH_SAVE_STRUCT(L5, NNH_SAVE_FIELD(inner, 1));
struct L6 {
    L5 inner;
};
NNH_SAVE_STRUCT(L6, NNH_SAVE_FIELD(inner, 1));
struct L7 {
    L6 inner;
};
NNH_SAVE_STRUCT(L7, NNH_SAVE_FIELD(inner, 1));
struct L8 {
    L7 inner;
};
NNH_SAVE_STRUCT(L8, NNH_SAVE_FIELD(inner, 1));
struct L9 {
    L8 inner;
};
NNH_SAVE_STRUCT(L9, NNH_SAVE_FIELD(inner, 1));
