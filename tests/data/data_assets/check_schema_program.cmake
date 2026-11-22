# cmake -DPROGRAM=<schema program> -DOUT=<description to write> -DCOMMITTED=<schema.nnhschema> -DROOT=<source root>
#       -P check_schema_program.cmake
#
# The tests' schema program (spec 014 T7.2, T6.3): writes exactly the committed fixtures' description (schema
# sources shown relative to ROOT there), so the fixtures can't drift from the compiled tests' schema; and run again
# on an unchanged schema, it leaves the file alone (restat: nothing is reconverted).

cmake_minimum_required(VERSION 3.25)
file(REMOVE "${OUT}")
execute_process(COMMAND "${PROGRAM}" "${OUT}" RESULT_VARIABLE code ERROR_VARIABLE err)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "the schema program failed (${code}):\n${err}")
endif()
file(READ "${OUT}" written)
file(READ "${COMMITTED}" committed)
string(REPLACE "\r\n" "\n" committed "${committed}")
string(REPLACE "${ROOT}/" "" relative "${written}")
if(NOT relative STREQUAL committed)
    message(FATAL_ERROR "${COMMITTED} is not the tests' schema any more: copy ${OUT} over it (sources relative "
                        "to the repository root)\n--- written\n${relative}--- committed\n${committed}")
endif()

file(TIMESTAMP "${OUT}" before "%Y%m%d%H%M%S")
execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 1.1)
execute_process(COMMAND "${PROGRAM}" "${OUT}" RESULT_VARIABLE code)
file(TIMESTAMP "${OUT}" after "%Y%m%d%H%M%S")
if(NOT code EQUAL 0 OR NOT before STREQUAL after)
    message(FATAL_ERROR "the schema program rewrote an unchanged description (${before} -> ${after})")
endif()
