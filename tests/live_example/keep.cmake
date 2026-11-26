# Spec 030 AC-8 (ctest live_keep_pc): keep.nnhs dumps three values, then keep_second.nnhs runs with --tunes <the saved
# dump> and finds them on the first frame. -DNNH=<nnh> -DSOURCE_DIR=<repo>.
file(GLOB _before "${SOURCE_DIR}/build/logs/live-*-tunes.toml")
execute_process(COMMAND ${NNH} run live --pc --no-build --script examples/live/tests/keep.nnhs
                WORKING_DIRECTORY ${SOURCE_DIR} RESULT_VARIABLE _r1 OUTPUT_VARIABLE _o1 ERROR_VARIABLE _o1)
message("${_o1}")
if(NOT _r1 EQUAL 0)
    message(FATAL_ERROR "live keep: run 1 failed")
endif()
file(GLOB _after "${SOURCE_DIR}/build/logs/live-*-tunes.toml")
list(REMOVE_ITEM _after ${_before})
list(LENGTH _after _n)
if(NOT _n EQUAL 1)
    message(FATAL_ERROR "live keep: expected one new dump, found ${_n}")
endif()
execute_process(COMMAND ${NNH} run live --pc --no-build --tunes ${_after} --script examples/live/tests/keep_second.nnhs
                WORKING_DIRECTORY ${SOURCE_DIR} RESULT_VARIABLE _r2 OUTPUT_VARIABLE _o2 ERROR_VARIABLE _o2)
message("${_o2}")
if(NOT _r2 EQUAL 0)
    message(FATAL_ERROR "live keep: run 2 failed")
endif()
message("live keep: PASS")
