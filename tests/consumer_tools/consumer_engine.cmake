# Preparing the engine for the consumer tests' ways (spec 032 AC-2), included by run_consumer.cmake.
#
#   consumer_copy_engine(<engine dir> <dest>)   a copy of the engine folder without build/, .git and the fixture's
#                                               own build folders (the layout of a source archive)
#   consumer_pack_engine(<engine dir> <work> <out_url> <out_sha256>)   that copy as <work>/nnhge.tar.gz, its file://
#                                               URL and SHA-256
#   consumer_listing(<dir> <out_var>)           names, sizes and times of every file below <dir> (AC-5)

function(consumer_copy_engine engine dest)
    file(REMOVE_RECURSE "${dest}")
    file(MAKE_DIRECTORY "${dest}")
    file(GLOB children RELATIVE "${engine}" "${engine}/*" "${engine}/.*")
    foreach(child IN LISTS children)
        if(child MATCHES "^(build|\\.git|\\.|\\.\\.)$")
            continue()
        endif()
        if(IS_DIRECTORY "${engine}/${child}")
            file(COPY "${engine}/${child}" DESTINATION "${dest}"
                 REGEX "/tests/consumer/(build|engine_pin\\.cmake|CMakeUserPresets\\.json)$" EXCLUDE)
        else()
            file(COPY "${engine}/${child}" DESTINATION "${dest}")
        endif()
    endforeach()
endfunction()

function(consumer_pack_engine engine work out_url out_sha256)
    consumer_copy_engine("${engine}" "${work}/pack/nnhge")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar czf "${work}/nnhge.tar.gz" nnhge
                    WORKING_DIRECTORY "${work}/pack" RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "consumer: packing the engine failed (${rc})")
    endif()
    file(REMOVE_RECURSE "${work}/pack")
    file(SHA256 "${work}/nnhge.tar.gz" sha)
    set(${out_url} "file://${work}/nnhge.tar.gz" PARENT_SCOPE) # file://E:/... (Windows) or file:///tmp/... (Linux)
    set(${out_sha256} "${sha}" PARENT_SCOPE)
endfunction()

function(consumer_listing dir out_var)
    file(GLOB_RECURSE files LIST_DIRECTORIES false RELATIVE "${dir}" "${dir}/*")
    list(SORT files)
    set(lines "")
    foreach(f IN LISTS files)
        file(SIZE "${dir}/${f}" size)
        file(TIMESTAMP "${dir}/${f}" time "%Y%m%d%H%M%S")
        string(APPEND lines "${f} ${size} ${time}\n")
    endforeach()
    set(${out_var} "${lines}" PARENT_SCOPE)
endfunction()
