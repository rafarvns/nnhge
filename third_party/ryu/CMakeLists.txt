# Ryu's f2s (see README.md): third-party C, compiled once, warnings off, the float table, asserts off.
add_library(nnh_ryu OBJECT ryu/f2s.c)
target_include_directories(nnh_ryu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(nnh_ryu PRIVATE RYU_FLOAT_FULL_TABLE NDEBUG)
if(MSVC)
    target_compile_options(nnh_ryu PRIVATE /w /fp:precise)
else()
    target_compile_options(nnh_ryu PRIVATE -w -ffp-contract=off)
endif()
