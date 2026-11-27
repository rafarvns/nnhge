# Runs a hot reload script with --pc (spec 031 T3.7, T7.5) and checks that it passed and that the game's folder is
# exactly as git has it afterwards (every swapped source put back). Arguments: -DNNH=<nnh> -DGAME=<name>
# -DSCRIPT=<path> -DSOURCE_DIR=<repo>. Prints "reload script: PASS" on success.
execute_process(COMMAND ${NNH} run ${GAME} --pc --no-build --script ${SCRIPT}
                WORKING_DIRECTORY ${SOURCE_DIR} OUTPUT_VARIABLE out ERROR_VARIABLE out RESULT_VARIABLE code)
message("${out}")
if(NOT code EQUAL 0 OR NOT out MATCHES "nnh: PASS")
    message(FATAL_ERROR "reload script: FAIL (exit ${code})")
endif()
execute_process(COMMAND git diff --name-only -- examples/${GAME} # the working tree against the index
                WORKING_DIRECTORY ${SOURCE_DIR} OUTPUT_VARIABLE dirty RESULT_VARIABLE git_code)
if(git_code EQUAL 0 AND NOT dirty STREQUAL "")
    message(FATAL_ERROR "reload script: FAIL: the game's folder was left changed:\n${dirty}")
endif()
message("reload script: PASS")
