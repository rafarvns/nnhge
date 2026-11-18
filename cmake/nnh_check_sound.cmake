# cmake -DSOUND=<file.wav|file.ogg> -P nnh_check_sound.cmake
#
# Fails, naming the file, unless SOUND is a sound the engine plays on every system (spec 008): a WAV
# with PCM 8 or 16-bit samples, or Ogg Vorbis; mono or stereo; 8000 to 48000 Hz. Reads only the header.

cmake_minimum_required(VERSION 3.25) # -P scripts don't inherit the project's policies
include(${CMAKE_CURRENT_LIST_DIR}/nnh_media_info.cmake)

if(NOT SOUND)
    message(FATAL_ERROR "nnh_check_sound: SOUND is required")
endif()
get_filename_component(_name "${SOUND}" NAME)
if(NOT EXISTS "${SOUND}")
    message(FATAL_ERROR "${_name}: file not found (${SOUND})")
endif()

function(_check_layout name channels rate)
    if(channels LESS 1 OR channels GREATER 2)
        message(FATAL_ERROR "${name} has ${channels} channels: sounds must be mono or stereo")
    endif()
    if(rate LESS 8000 OR rate GREATER 48000)
        message(FATAL_ERROR "${name} is at ${rate} Hz: sounds must be 8000 to 48000 Hz (32000 or less "
                            "saves memory on the 3DS)")
    endif()
endfunction()

file(SIZE "${SOUND}" _size)
file(READ "${SOUND}" _head LIMIT 4096 HEX)
string(LENGTH "${_head}" _hex_length)
math(EXPR _have "${_hex_length} / 2")

if(_name MATCHES "\\.[wW][aA][vV]$")
    nnh_wav_info("${SOUND}" _wav)
    if(NOT _wav_ERROR STREQUAL "")
        message(FATAL_ERROR "${_name}: ${_wav_ERROR}")
    endif()
    set(_format ${_wav_FORMAT})
    set(_bits ${_wav_BITS})
    set(_channels ${_wav_CHANNELS})
    set(_rate ${_wav_RATE})
    if(NOT _format EQUAL 1)
        message(FATAL_ERROR "${_name}: WAV encoding ${_format} isn't supported: save it as PCM 16-bit "
                            "(or Ogg Vorbis)")
    endif()
    if(NOT _bits EQUAL 8 AND NOT _bits EQUAL 16)
        message(FATAL_ERROR "${_name} is ${_bits}-bit: save it as PCM 16-bit (or 8-bit)")
    endif()
    _check_layout("${_name}" ${_channels} ${_rate})
elseif(_name MATCHES "\\.[oO][gG][gG]$")
    string(SUBSTRING "${_head}" 0 8 _capture)
    if(_have LESS 58 OR NOT _capture STREQUAL "4f676753") # "OggS"
        message(FATAL_ERROR "${_name}: not an Ogg file")
    endif()
    # First page: 27-byte header + lacing values, then the identification packet.
    _nnh_le_uint("${_head}" 26 1 _segments)
    math(EXPR _p "27 + ${_segments}")
    math(EXPR _hex_p "${_p} * 2")
    string(SUBSTRING "${_head}" ${_hex_p} 16 _packet)
    if(_packet MATCHES "^4f70757348656164") # "OpusHead"
        message(FATAL_ERROR "${_name} is Ogg Opus: only Ogg Vorbis is supported")
    endif()
    string(SUBSTRING "${_packet}" 0 14 _vorbis)
    if(NOT _vorbis STREQUAL "01766f72626973") # 0x01 "vorbis"
        message(FATAL_ERROR "${_name}: an Ogg file without Vorbis audio")
    endif()
    _nnh_le_uint("${_head}" ${_p}+11 1 _channels)
    _nnh_le_uint("${_head}" ${_p}+12 4 _rate)
    _check_layout("${_name}" ${_channels} ${_rate})
else()
    message(FATAL_ERROR "nnh_check_sound: ${_name} is not .wav or .ogg")
endif()
