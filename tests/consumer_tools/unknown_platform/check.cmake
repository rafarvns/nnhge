# cmake -DENGINE=<engine> -DWORK=<dir> -DMAKE_PROGRAM=<ninja> -DPLATFORM_VALUE=<value or <unset>> -DEXPECT=<regex> -P check.cmake
# Configures ./CMakeLists.txt and expects the FR-6 failure: the message matching EXPECT, no build.ninja (spec 032 AC-12).
file(REMOVE_RECURSE "${WORK}")
execute_process(COMMAND ${CMAKE_COMMAND} -S "${CMAKE_CURRENT_LIST_DIR}" -B "${WORK}" -G Ninja
                        "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}" "-DENGINE=${ENGINE}" "-DPLATFORM_VALUE=${PLATFORM_VALUE}"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}")
if(rc EQUAL 0)
    message(FATAL_ERROR "unknown platform '${PLATFORM_VALUE}': the configure succeeded\n${out}")
endif()
if(NOT flat MATCHES "${EXPECT}")
    message(FATAL_ERROR "unknown platform '${PLATFORM_VALUE}': no message matching '${EXPECT}'\n${out}")
endif()
if(EXISTS "${WORK}/build.ninja")
    message(FATAL_ERROR "unknown platform '${PLATFORM_VALUE}': build.ninja was generated")
endif()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "unknown platform '${PLATFORM_VALUE}': refused as expected")
