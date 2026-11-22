# A test "game" with a broken data file (spec 014 T7.6, AC-3): never built by default; ctest
# data_build_fails_with_file_line builds its target nnh_test_broken_data_assets and expects the converter's
# `assets/enemies/goblin.enemy:<line>: <reason>`.

add_custom_target(nnh_test_broken_data)
nnh_use_data_schema(nnh_test_broken_data "${NNH_TESTS_DATA_SCHEMA}" nnh_tests_data_description)
nnh_add_assets(nnh_test_broken_data assets)
