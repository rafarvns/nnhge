# ctest nnh_template_smoke (spec 033 FR-16, T3.3): the template's own test script, run by nnh against the check game
# nnh_template_check (the template's sources and assets built inside the engine's PC build), headless, as a game
# project's developer would run it: nnh: PASS, exit 0, the screenshot of both screens saved.
#   cmake -DNNH=<nnh executable> -DSOURCE_DIR=<repository> [-DSCRIPT=<.nnhs>] -P template_smoke.cmake
# The environment (headless SDL, NNH_PRESET) comes from the test's properties.

if(NOT SCRIPT)
    set(SCRIPT "${SOURCE_DIR}/tools/nnh/template/tests/smoke.nnhs")
endif()
execute_process(
    COMMAND "${NNH}" run nnh_template_check --pc --no-build --script "${SCRIPT}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE code
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    TIMEOUT 120)
message("${out}${err}")
if(NOT code EQUAL 0)
    message(FATAL_ERROR "nnh exited with ${code}, not 0")
endif()
if(NOT out MATCHES "nnh: PASS \\(")
    message(FATAL_ERROR "no 'nnh: PASS' line")
endif()
string(REGEX MATCHALL "screenshot moved -> [^\n]+\\.png" saved "${out}")
list(LENGTH saved count)
if(NOT count EQUAL 2)
    message(FATAL_ERROR "expected the two screens of screenshot 'moved', nnh reported ${count}")
endif()
foreach(line IN LISTS saved)
    string(REGEX REPLACE "^.* -> " "" path "${line}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "missing ${path}")
    endif()
endforeach()
message(STATUS "the template's smoke script passed")
