# ctest platformer_replay_matches_script (spec 012 T7.3, FR-10): plays the platformer's replay in the simulator
# (headless, PLATFORMER_DEMO=replay from the test's environment) and checks that its checksum lines are exactly the
# ones examples/platformer/tests/level.nnhs expects, so the console script always holds the PC's numbers and two runs
# (the one that wrote the script and this one) agree.
#   cmake -DGAME=<platformer executable> -DSCRIPT=<level.nnhs> -P replay_matches_script.cmake

execute_process(COMMAND "${GAME}" RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 90)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "the platformer exited with ${code}:\n${out}${err}")
endif()
set(line_regex "replay: [0-9]+ [a-z0-9]+ x -?[0-9]+ y -?[0-9]+ coins [0-9]+ sum [0-9A-F]+|replay: done [0-9]+ frames sum [0-9A-F]+")
string(REGEX MATCHALL "${line_regex}" got "${out}")

file(READ "${SCRIPT}" script)
# Checkpoint lines are expected whole; the last one up to its colon (its verdict is checked by the script only:
# frame times are not repeatable).
string(REGEX MATCHALL "expect \"(${line_regex})[^\"]*\"" expects "${script}")
set(want "")
foreach(expect IN LISTS expects)
    string(REGEX MATCH "${line_regex}" line "${expect}")
    list(APPEND want "${line}")
endforeach()

list(LENGTH got got_count)
list(LENGTH want want_count)
string(REGEX MATCH "60 Hz [^\n]*" verdict "${out}")
message(STATUS "${got_count} checksum lines from the simulator, ${want_count} in ${SCRIPT}; ${verdict}")
if(want_count LESS 2)
    message(FATAL_ERROR "${SCRIPT} expects no replay checksum lines")
endif()
if(NOT got STREQUAL want)
    string(REPLACE ";" "\n  " got_text "${got}")
    string(REPLACE ";" "\n  " want_text "${want}")
    message(FATAL_ERROR "the replay's checksum lines differ from the script's.\nsimulator:\n  ${got_text}\n"
                        "script:\n  ${want_text}\nIf the change is intended, copy the simulator's lines into the script.")
endif()
