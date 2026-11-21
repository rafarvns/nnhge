# ctest nnh_script_pc_smoke (spec 013 T4.5, FR-4): runs examples/input/tests/remote.nnhs against the simulator
# through nnh, as a developer would, and checks the verdict and the four screenshots (sizes from the PNG header).
#   cmake -DNNH=<nnh executable> -DSOURCE_DIR=<repository> -P script_pc_smoke.cmake
# The environment (headless SDL, NNH_PRESET) comes from the test's properties.

execute_process(
    COMMAND "${NNH}" run input --pc --no-build --script examples/input/tests/remote.nnhs
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

string(REGEX MATCHALL "screenshot [A-Za-z0-9_-]+ -> [^\n]+\\.png" saved "${out}")
list(LENGTH saved count)
if(NOT count EQUAL 4)
    message(FATAL_ERROR "expected 4 screenshots, nnh reported ${count}")
endif()
foreach(line IN LISTS saved)
    string(REGEX REPLACE "^.* -> " "" path "${line}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "missing ${path}")
    endif()
    file(READ "${path}" header LIMIT 24 HEX)
    string(SUBSTRING "${header}" 0 16 signature)
    string(SUBSTRING "${header}" 32 8 width)
    string(SUBSTRING "${header}" 40 8 height)
    if(path MATCHES "-top\\.png$")
        set(want_width 00000190) # 400
    else()
        set(want_width 00000140) # 320
    endif()
    if(NOT signature STREQUAL "89504e470d0a1a0a" OR NOT width STREQUAL want_width OR NOT height STREQUAL "000000f0")
        message(FATAL_ERROR "${path}: not a ${want_width}x000000f0 PNG (header ${header})")
    endif()
endforeach()
message(STATUS "4 screenshots of the right sizes")
