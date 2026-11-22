# A test "game" whose assets are this folder's fixtures, converted by the real pipeline (nnh_assets.cmake) with
# the tests' schema (spec 014 T7.3, T7.5): tests/CMakeLists.txt sets NNH_TESTS_DATA_SCHEMA and makes nnh_tests
# depend on the target; test_round_trip.cpp loads the packed folder <binary dir>/assets.

add_custom_target(nnh_test_data)
nnh_use_data_schema(nnh_test_data "${NNH_TESTS_DATA_SCHEMA}" nnh_tests_data_description)
nnh_add_assets(nnh_test_data assets)
