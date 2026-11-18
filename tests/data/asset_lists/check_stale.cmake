# ctest helper: with FOLDER, the index script removes outputs of assets that are no longer listed and
# keeps the listed ones (run with plain `cmake -P`, so it also checks the script sets its own policies).
set(folder ${WORK}/assets)
file(REMOVE_RECURSE ${WORK})
file(WRITE ${folder}/art/hero.png "kept")
file(WRITE ${folder}/levels/level1.txt "kept")
file(WRITE ${folder}/art/old.png "stale")
execute_process(COMMAND ${CMAKE_COMMAND} -DLIST=${DIR}/valid.list -DOUT=${folder}/.nnh_assets -DFOLDER=${folder}
                        -P ${SCRIPT}
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "index script failed")
endif()
foreach(kept art/hero.png levels/level1.txt .nnh_assets)
    if(NOT EXISTS ${folder}/${kept})
        message(FATAL_ERROR "removed a listed output: ${kept}")
    endif()
endforeach()
if(EXISTS ${folder}/art/old.png)
    message(FATAL_ERROR "kept a stale output: art/old.png")
endif()
