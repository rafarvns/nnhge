# stb_truetype (see README.md): third-party C, host-only (nnh-assetc), warnings off, no fused multiply-add.
add_library(nnh_stb_truetype OBJECT stb_truetype.c)
# Compiled as C++ (the host tool's project has only a C++ compiler: the 3DS presets build it with NNH_HOST_CXX).
set_source_files_properties(stb_truetype.c PROPERTIES LANGUAGE CXX)
target_include_directories(nnh_stb_truetype PUBLIC ${CMAKE_CURRENT_LIST_DIR})
if(MSVC)
    target_compile_options(nnh_stb_truetype PRIVATE /w /fp:precise)
else()
    target_compile_options(nnh_stb_truetype PRIVATE -w -ffp-contract=off)
endif()
