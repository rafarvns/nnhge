# Unit tests for platform-independent engine code (docs/process/tdd.md).
# Layout mirrors engine/src: tests/<module>/test_<concept>.cpp. List new files here.

add_executable(nnh_tests
    framework/test_main.cpp
    input/test_input_state.cpp
)
target_include_directories(nnh_tests PRIVATE
    framework
    ${PROJECT_SOURCE_DIR}/engine/src
    ${PROJECT_SOURCE_DIR}/engine/include)
add_test(NAME unit COMMAND nnh_tests)
