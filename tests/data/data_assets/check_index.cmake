# cmake -DASSETC=<nnh-assetc> -DSCHEMA=<description> -DCASE=<assets folder> -DSHOW=<folder shown> -DWORK=<dir>
#       [-DEXPECT=<file>] -P check_index.cmake
#
# Runs the data pipeline of nnh_assets.cmake by hand on a folder (spec 014 T6.3, AC-3): `nnh-assetc data` on
# every file that isn't an image or a sound, then `nnh-assetc data-index`. With EXPECT, the index step must fail
# and print exactly that file's text; without, everything must succeed.

cmake_minimum_required(VERSION 3.25)
foreach(var ASSETC SCHEMA CASE SHOW WORK)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "check_index: ${var} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(GLOB_RECURSE files LIST_DIRECTORIES false RELATIVE "${CASE}" "${CASE}/*")
list(SORT files)
set(names "")
set(sidecars "")
foreach(name IN LISTS files)
    list(APPEND names "${name}")
    if(name MATCHES "\\.(png|wav|ogg)$")
        continue()
    endif()
    get_filename_component(dir "${WORK}/out/${name}" DIRECTORY)
    get_filename_component(refs_dir "${WORK}/refs/${name}" DIRECTORY)
    file(MAKE_DIRECTORY "${dir}" "${refs_dir}")
    execute_process(COMMAND "${ASSETC}" data "${CASE}/${name}" "${WORK}/out/${name}" --schema "${SCHEMA}"
                            --refs "${WORK}/refs/${name}.refs" --name "${name}" --show "${SHOW}/${name}"
                    RESULT_VARIABLE code ERROR_VARIABLE err)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "data step failed:\n${err}")
    endif()
    list(APPEND sidecars "${WORK}/refs/${name}.refs")
endforeach()
string(REPLACE ";" "\n" names "${names}")
string(REPLACE ";" "\n" sidecars "${sidecars}")
file(WRITE "${WORK}/assets.list" "${names}\n")
file(WRITE "${WORK}/refs.list" "${sidecars}\n")

execute_process(COMMAND "${ASSETC}" data-index --out "${WORK}/out/.nnh_data" --schema "${SCHEMA}"
                        --assets-list "${WORK}/assets.list" --refs-list "${WORK}/refs.list" --show "${SHOW}"
                RESULT_VARIABLE code ERROR_VARIABLE err)
string(REPLACE "\r\n" "\n" err "${err}")
if(NOT EXPECT)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "the index step failed:\n${err}")
    endif()
    return()
endif()
file(READ "${EXPECT}" expected)
string(REPLACE "\r\n" "\n" expected "${expected}")
if(code EQUAL 0)
    message(FATAL_ERROR "${SHOW} should fail with:\n${expected}but the index was written")
elseif(NOT err STREQUAL expected)
    message(FATAL_ERROR "${SHOW}: expected\n${expected}got\n${err}")
endif()
