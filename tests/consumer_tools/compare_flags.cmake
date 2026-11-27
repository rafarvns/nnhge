# Compile commands compared across builds (spec 032 FR-7, FR-8, AC-7), included by run_consumer.cmake.
#
#   consumer_compile_line(<build dir> <target> <source regex> <out_var>)
#       The compile command Ninja runs for <target>'s source matching <source regex> (from `ninja -t commands`).
#   consumer_flags(<line> <build dir> <out_var>)
#       The command's flags only: the build folder replaced by <B>, the game's NNH_GAME_TITLE definition, the
#       dependency file's name (-MT/-MF) and the output/input part (-o ... -c ...) removed, so two games' commands
#       compare equal when their settings are.

function(consumer_compile_line build target source_regex out_var)
    execute_process(COMMAND "${MAKE_PROGRAM}" -C "${build}" -t commands ${target}
                    OUTPUT_VARIABLE all RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "consumer: ninja -t commands ${target} failed in ${build}")
    endif()
    string(REPLACE "\n" ";" lines "${all}")
    foreach(line IN LISTS lines)
        if(line MATCHES "${source_regex}" AND line MATCHES " -c ")
            set(${out_var} "${line}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "consumer: no compile command of ${target} matching ${source_regex} in ${build}")
endfunction()

function(consumer_flags line build out_var)
    string(REPLACE "\\" "/" line "${line}")
    string(REPLACE "\\" "/" build "${build}")
    string(REPLACE "${build}" "<B>" line "${line}")
    string(REGEX REPLACE " -DNNH_GAME_TITLE=[^ ]*( [^-][^ ]*)*" "" line "${line}")
    string(REGEX REPLACE " -M[TF] [^ ]+" "" line "${line}") # the dependency file's name: the object's path
    string(REGEX REPLACE " -o .*$" "" line "${line}")
    set(${out_var} "${line}" PARENT_SCOPE)
endfunction()
