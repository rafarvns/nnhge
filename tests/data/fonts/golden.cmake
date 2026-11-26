# font_fixtures_golden (spec 029 T4.9, AC-2): every font fixture of this folder converted by nnh-assetc, and the
# SHA-256 of each .nnhfont compared with the values below — the same bytes on every compiler and host (WinLibs GCC, the
# build VM's GCC 13/14 and clang 18, the 32-bit host build). Usage:
#   cmake -DASSETC=<nnh-assetc> -DFONTS=<this folder> -DOUT=<scratch folder> -P golden.cmake
# A changed converter changes these on purpose: rerun with -DRECORD=1 and paste the printed lines here.
cmake_minimum_required(VERSION 3.25) # -P scripts don't inherit the project's policies

set(_expected
    body_8.font=90b58e819a24950b5471a4c8bb30fcee8a2eaeb97e93e4d53b21253750f34677
    body_12.font=9b485c612407074e2f7dc3dac918e9a21ef575dd42ac8ccdf8bfcedd5dc5a6e1
    body_16.font=3a6b8870d5e0469b9cfaefb8cb16c90d3c00b3d41c079b6813acbc0b82e2bd32
    body_32.font=c2a233e952a045fa8410dfa6e9509c845eb7dad55d10c86e6255308dc59be00e
    cff_16.font=b02b1829f301addf43b6e9a762df57e430ff05c198d7845529a2d1c44316bfb5
    outline_16.font=b06382e82950056a0cde5c5c4032baa5b159c5960cab14f349ff861ac55716fb
    outline3_16.font=b14e580595d4f7b2cccb683f0e1bf32880e62c8d2ce26b2dfd51bc82ed7d8a99
    pixel_12.font=3bfddbcaebc3c3169aa37e912a8617c23967c4ce0ade91b0ded87d003437bfa9
    mono.fnt=9a281ed89fb7c8305bd8ed80880388c794fb2eb2233935e676eb5bd1c3380e44
    mono_xml.fnt=b5b239725e9cca49eb90f86b5d2973adebcfc16d4bb6804f394370d5024922b2
    colour.fnt=667b18383a3c4fdcc21b704f1a8c3fa2de5d15dcd60fa153c5695f99463b4bf6)

file(MAKE_DIRECTORY "${OUT}")
set(_failed "")
foreach(_entry IN LISTS _expected)
    string(REPLACE "=" ";" _pair "${_entry}")
    list(GET _pair 0 _file)
    list(GET _pair 1 _want)
    get_filename_component(_name "${_file}" NAME_WE)
    execute_process(COMMAND "${ASSETC}" font "${FONTS}/${_file}" "${OUT}/${_name}.nnhfont" --assets "${FONTS}"
                            --show "fonts/${_file}"
                    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        list(APPEND _failed "${_file}: nnh-assetc failed: ${_err}")
        continue()
    endif()
    file(SHA256 "${OUT}/${_name}.nnhfont" _got)
    if(RECORD)
        message("    ${_file}=${_got}")
    elseif(NOT _got STREQUAL _want)
        list(APPEND _failed "${_file}: ${_got} (expected ${_want})")
    endif()
endforeach()
if(_failed)
    string(REPLACE ";" "\n" _text "${_failed}")
    message(FATAL_ERROR "font fixtures differ:\n${_text}")
endif()
message("font fixtures golden: PASS")
