# Unit tests for platform-independent core code. Plain asserts, no framework (fast to build).
add_executable(nnh_tests test_main.cpp test_input.cpp)
target_include_directories(nnh_tests PRIVATE ${PROJECT_SOURCE_DIR}/engine/src ${PROJECT_SOURCE_DIR}/engine/include)
add_test(NAME core COMMAND nnh_tests)
