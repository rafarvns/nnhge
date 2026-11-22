// Must not compile (spec 014 FR-1, FR-14; ctest data_schema_rejects_too_many_fields): 65 fields in one type.
#include "nnh/data.hpp"

#define M8(p) int p##0, p##1, p##2, p##3, p##4, p##5, p##6, p##7;
#define F8(p)                                                                                                      \
    NNH_DATA_FIELD(p##0), NNH_DATA_FIELD(p##1), NNH_DATA_FIELD(p##2), NNH_DATA_FIELD(p##3), NNH_DATA_FIELD(p##4),  \
        NNH_DATA_FIELD(p##5), NNH_DATA_FIELD(p##6), NNH_DATA_FIELD(p##7)

struct Big {
    M8(a) M8(b) M8(c) M8(d) M8(e) M8(f) M8(g) M8(h) int extra;
};
NNH_DATA_TYPE(Big, "big", F8(a), F8(b), F8(c), F8(d), F8(e), F8(f), F8(g), F8(h), NNH_DATA_FIELD(extra));
