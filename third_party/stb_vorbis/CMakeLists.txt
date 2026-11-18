# stb_vorbis (see README.md): third-party C, compiled once, warnings off, push-data API only.
add_library(nnh_stb_vorbis OBJECT stb_vorbis.c)
target_compile_definitions(nnh_stb_vorbis PRIVATE STB_VORBIS_NO_PULLDATA_API)
if(MSVC)
    target_compile_options(nnh_stb_vorbis PRIVATE /w)
else()
    target_compile_options(nnh_stb_vorbis PRIVATE -w)
    # Sanitizer builds (Linux checks): stb_vorbis builds Ogg CRCs with `byte << 24` on int, which UBSan
    # reports as shift-base; the result is right on every compiler we use. Every other check stays on.
    if(CMAKE_C_FLAGS MATCHES "sanitize=[^ ]*undefined")
        target_compile_options(nnh_stb_vorbis PRIVATE -fno-sanitize=shift-base)
    endif()
endif()
