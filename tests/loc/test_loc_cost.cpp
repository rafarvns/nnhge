// Localization costs nothing while playing (spec 029 FR-34): tr, format and the queries allocate nothing after start.

#include "loc_fixture.hpp"
#include "nnh/localization.hpp"
#include "nnh/memory.hpp"
#include "test.hpp"

using namespace loc_test;
using namespace font_test;

NNH_TEST(loc_tr_and_format_allocate_nothing) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    four_languages().install();
    nnh::platform_fake::set_system_language("ru");
    CHECK(nnh::loc::start().ok());
    const int live = nnh::mem::live_allocations();
    char out[96];
    int bytes = 0;
    for (int i = 0; i < 5000; ++i) {
        bytes += static_cast<int>(nnh::loc::tr("menu.new_game")[0] != 0);
        bytes += nnh::loc::format(out, sizeof out, "items.count", nnh::loc::arg("count", i));
        bytes += nnh::loc::find("ja");
    }
    CHECK(bytes > 0);
    CHECK(nnh::mem::live_allocations() == live);
}
