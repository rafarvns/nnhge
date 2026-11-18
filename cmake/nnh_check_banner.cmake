# cmake [-DBANNER=<file.png>] [-DAUDIO=<file.wav>] [-DSTAMP=<file>] -P nnh_check_banner.cmake
#
# Fails, naming the file, unless BANNER is a 256x128 PNG and AUDIO a 16-bit PCM WAV (mono or stereo) of
# at most 3 seconds: what the HOME Menu shows and plays for an installed .cia (spec 009).

cmake_minimum_required(VERSION 3.25) # -P scripts don't inherit the project's policies
include(${CMAKE_CURRENT_LIST_DIR}/nnh_media_info.cmake)

if(BANNER)
    get_filename_component(_name "${BANNER}" NAME)
    if(NOT EXISTS "${BANNER}")
        message(FATAL_ERROR "${_name}: file not found (${BANNER})")
    endif()
    nnh_png_size("${BANNER}" _w _h)
    if(_w STREQUAL "")
        message(FATAL_ERROR "${_name}: not a PNG file (the banner must be a 256x128 PNG)")
    endif()
    if(NOT _w EQUAL 256 OR NOT _h EQUAL 128)
        message(FATAL_ERROR "${_name} is ${_w}x${_h}: the banner must be a 256x128 PNG")
    endif()
endif()

if(AUDIO)
    get_filename_component(_name "${AUDIO}" NAME)
    if(NOT EXISTS "${AUDIO}")
        message(FATAL_ERROR "${_name}: file not found (${AUDIO})")
    endif()
    nnh_wav_info("${AUDIO}" _wav)
    if(NOT _wav_ERROR STREQUAL "")
        message(FATAL_ERROR "${_name}: ${_wav_ERROR}")
    endif()
    if(NOT _wav_FORMAT EQUAL 1 OR NOT _wav_BITS EQUAL 16)
        message(FATAL_ERROR "${_name} is ${_wav_BITS}-bit: the banner sound must be a 16-bit PCM WAV")
    endif()
    if(_wav_CHANNELS LESS 1 OR _wav_CHANNELS GREATER 2 OR _wav_RATE LESS 8000 OR _wav_RATE GREATER 48000)
        message(FATAL_ERROR "${_name}: the banner sound must be mono or stereo, 8000 to 48000 Hz")
    endif()
    if(_wav_DATA_BYTES STREQUAL "")
        message(FATAL_ERROR "${_name}: no data chunk in the first 4 KB")
    endif()
    math(EXPR _byte_rate "${_wav_RATE} * ${_wav_CHANNELS} * 2")
    math(EXPR _tenths "(${_wav_DATA_BYTES} * 10 + ${_byte_rate} - 1) / ${_byte_rate}") # rounded up
    math(EXPR _max_bytes "3 * ${_byte_rate}")
    if(_wav_DATA_BYTES GREATER _max_bytes)
        math(EXPR _s "${_tenths} / 10")
        math(EXPR _t "${_tenths} % 10")
        message(FATAL_ERROR "${_name} lasts ${_s}.${_t} s: the banner sound can last at most 3 s")
    endif()
endif()

if(STAMP)
    file(WRITE "${STAMP}" "ok\n")
endif()
