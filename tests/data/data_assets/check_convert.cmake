# cmake -DASSETC=<nnh-assetc> -DSCHEMA=<description> -DINPUT=<fixture> -DSHOW=<path shown> -DWORK=<dir>
#       -P check_convert.cmake
#
# Converts one data fixture (spec 014 T6.3, AC-3). A fixture whose first lines are `# expect: <message>` must
# fail and print exactly those messages, one per line, in that order; any other fixture must convert.

cmake_minimum_required(VERSION 3.25)
foreach(var ASSETC SCHEMA INPUT SHOW WORK)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "check_convert: ${var} is required")
    endif()
endforeach()

file(READ "${INPUT}" head LIMIT 4096)
set(expected "")
while(head MATCHES "^# expect: ([^\n]*)\n")
    string(APPEND expected "${CMAKE_MATCH_1}\n")
    string(LENGTH "# expect: ${CMAKE_MATCH_1}\n" used)
    string(SUBSTRING "${head}" ${used} -1 head)
endwhile()

get_filename_component(name "${INPUT}" NAME)
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${ASSETC}" data "${INPUT}" "${WORK}/${name}" --schema "${SCHEMA}" --refs "${WORK}/${name}.refs"
                        --name "${name}" --show "${SHOW}"
                RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_QUIET)
string(REPLACE "\r\n" "\n" err "${err}")
if(expected STREQUAL "")
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "${SHOW} should convert, but:\n${err}")
    endif()
elseif(code EQUAL 0)
    message(FATAL_ERROR "${SHOW} should fail with:\n${expected}but it converted")
elseif(NOT err STREQUAL expected)
    message(FATAL_ERROR "${SHOW}: expected\n${expected}got\n${err}")
endif()
