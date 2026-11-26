// Missing characters logged once (spec 029 FR-11): per (font slot, code point), 128 remembered, then one "log full" line;
// Release logs nothing.

#include "../font/font_load_fixture.hpp"
#include "nnh/build.hpp"
#include "test.hpp"
#include "text/text_missing.hpp"

using font_test::LogLines;

NNH_TEST(text_missing_logs_each_code_point_once_per_font) {
    nnh::detail::text_missing_reset();
    LogLines warnings(nnh::LogLevel::Warn);
    nnh::detail::text_missing(3, 0x4E00, "fonts/body");
    nnh::detail::text_missing(3, 0x4E00, "fonts/body");
    nnh::detail::text_missing(4, 0x4E00, "fonts/title");
    const int on = nnh::build::kDevToolsEnabled ? 1 : 0;
    CHECK(warnings.count("U+4E00 not in fonts/body") == on);
    CHECK(warnings.count("U+4E00 not in fonts/title") == on);
}

NNH_TEST(text_missing_says_once_when_its_log_is_full) {
    nnh::detail::text_missing_reset();
    LogLines warnings(nnh::LogLevel::Warn);
    for (int i = 0; i < 200; ++i) nnh::detail::text_missing(0, 0x3000 + static_cast<char32_t>(i), "f");
    const int on = nnh::build::kDevToolsEnabled ? 1 : 0;
    CHECK(warnings.count("not in f") == 128 * on);
    CHECK(warnings.count("more missing characters (log full)") == on);
    nnh::detail::text_missing_reset();
}
