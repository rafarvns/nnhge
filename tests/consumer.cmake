# The consumer tests (spec 032 FR-16, plan "Test fixture"): tests/consumer, a game project, built against this engine.
# Included by tests/CMakeLists.txt in PC presets. Labels:
#   consumer       the local way (FETCHCONTENT_SOURCE_DIR_NNHGE = this engine folder) and the unknown-platform checks;
#                  `ctest -LE consumer` skips them while developing
#   consumer-full  the other ways (archive, override, submodule, pruned, wrong hash, options ON): one engine build each;
#                  run by scripts/vm_check.sh final and the release checklist (`ctest -L consumer-full`)
# One engine build at a time (RESOURCE_LOCK consumer); each in build/<preset>/consumer/<way>.

set(_consumer_tools "${CMAKE_CURRENT_SOURCE_DIR}/consumer_tools")
set(_consumer_args
    -DENGINE=${PROJECT_SOURCE_DIR} -DPARENT=${PROJECT_BINARY_DIR}
    "-DGENERATOR=${CMAKE_GENERATOR}" "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
    "-DC_COMPILER=${CMAKE_C_COMPILER}" "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}" "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCCACHE=${NNH_CCACHE}")
# The parent's SDL3 download (cmake/nnh_sdl3.cmake): reused, so the tests download nothing.
include(FetchContent)
foreach(_sdl nnh_sdl3_prebuilt nnh_sdl3)
    FetchContent_GetProperties(${_sdl})
    if(${_sdl}_SOURCE_DIR)
        list(APPEND _consumer_args "-DSDL3_SOURCE=${${_sdl}_SOURCE_DIR}")
    endif()
endforeach()
# The self-check line every way and flavour prints (tests/consumer/game/src/self_check.cpp; also the console's).
set(_consumer_line "")

function(_nnh_consumer_test name way label)
    add_test(NAME ${name}
             COMMAND ${CMAKE_COMMAND} -DWAY=${way} -DWORK=${PROJECT_BINARY_DIR}/consumer/${way} ${_consumer_args}
                     "-DEXPECT_LINE=${_consumer_line}" -P ${_consumer_tools}/run_consumer.cmake)
    set_tests_properties(${name} PROPERTIES LABELS ${label} TIMEOUT 1200 RESOURCE_LOCK consumer)
endfunction()

_nnh_consumer_test(consumer local consumer)

# FR-6, AC-12: an empty or unsupported platform fails the configure with the game, the value and the supported systems.
foreach(_case "empty;<unset>;nnh_add_game[(]probe_game[)]: unknown platform \"\" [(]empty, supported: pc, 3ds[)]"
              "unknown;wii;nnh_add_game[(]probe_game[)]: unknown platform \"wii\" [(]supported: pc, 3ds[)]")
    list(GET _case 0 _name)
    list(GET _case 1 _value)
    list(GET _case 2 _expect)
    add_test(NAME consumer_platform_${_name}
             COMMAND ${CMAKE_COMMAND} -DENGINE=${PROJECT_SOURCE_DIR} -DWORK=${PROJECT_BINARY_DIR}/consumer/platform_${_name}
                     "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" "-DPLATFORM_VALUE=${_value}" "-DEXPECT=${_expect}"
                     -P ${_consumer_tools}/unknown_platform/check.cmake)
    set_tests_properties(consumer_platform_${_name} PROPERTIES LABELS consumer TIMEOUT 120)
endforeach()
