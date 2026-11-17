# nnh_engine_options(<target>)
# Compile settings shared by every engine library (core and platform libraries):
# C++20, warnings, and no exceptions/RTTI (docs/tech/cpp.md).

function(nnh_engine_options target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /GR- /EHs-c-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti)
    endif()
endfunction()
