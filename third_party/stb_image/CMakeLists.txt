# stb_image (see README.md): third-party C, host-only (nnh-assetc), PNG only, warnings off.
add_library(nnh_stb_image OBJECT stb_image.c)
# Compiled as C++ (the host tool's project has only a C++ compiler: the 3DS presets build it with NNH_HOST_CXX).
set_source_files_properties(stb_image.c PROPERTIES LANGUAGE CXX)
target_include_directories(nnh_stb_image PUBLIC ${CMAKE_CURRENT_LIST_DIR})
if(MSVC)
    target_compile_options(nnh_stb_image PRIVATE /w)
else()
    target_compile_options(nnh_stb_image PRIVATE -w)
endif()
