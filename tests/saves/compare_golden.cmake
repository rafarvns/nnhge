# The saves example ships byte-identical copies of the golden files (spec 027 AC-2, AC-6): ctest
# saves_golden_copies_match. Usage: cmake -DA=<tests/data/saves> -DB=<examples/saves/assets/golden> -P compare_golden.cmake
foreach(version 1 2 3)
    execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${A}/v${version}.sav ${B}/v${version}.sav
                    RESULT_VARIABLE different)
    if(different)
        message(FATAL_ERROR "v${version}.sav differs between ${A} and ${B}: copy tests/data/saves/*.sav again")
    endif()
endforeach()
message(STATUS "saves golden copies: identical")
