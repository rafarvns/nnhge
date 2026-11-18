# cmake -DSOUND=<file.wav|file.ogg> -P nnh_check_sound.cmake
#
# Fails, naming the file, unless SOUND is a sound the engine plays on every system (spec 008): a WAV
# with PCM 8 or 16-bit samples, or Ogg Vorbis; mono or stereo; 8000 to 48000 Hz. Reads only the header.

cmake_minimum_required(VERSION 3.25) # -P scripts don't inherit the project's policies

if(NOT SOUND)
    message(FATAL_ERROR "nnh_check_sound: SOUND is required")
endif()
get_filename_component(_name "${SOUND}" NAME)
if(NOT EXISTS "${SOUND}")
    message(FATAL_ERROR "${_name}: file not found (${SOUND})")
endif()

# Reads a little-endian unsigned integer of `bytes` bytes at byte `offset` of the hex string `hex`.
function(_le_uint hex offset bytes out)
    set(value 0)
    math(EXPR last "${bytes} - 1")
    foreach(i RANGE ${last} 0 -1)
        math(EXPR at "(${offset} + ${i}) * 2")
        string(SUBSTRING "${hex}" ${at} 2 byte)
        math(EXPR value "(${value} << 8) | 0x${byte}")
    endforeach()
    set(${out} ${value} PARENT_SCOPE)
endfunction()

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
    string(SUBSTRING "${_head}" 0 8 _riff)
    if(_have LESS 12 OR NOT _riff STREQUAL "52494646")
        message(FATAL_ERROR "${_name}: not a WAV file")
    endif()
    # Walk the chunks to `fmt ` (they may come in any order before `data`).
    set(_at 12)
    set(_found FALSE)
    math(EXPR _last "${_have} - 34") # room for a fmt chunk header and its extensible fields
    while(_at LESS_EQUAL _last)
        math(EXPR _hex_at "${_at} * 2")
        string(SUBSTRING "${_head}" ${_hex_at} 8 _id)
        _le_uint("${_head}" ${_at}+4 4 _chunk_size)
        if(_id STREQUAL "666d7420") # "fmt "
            math(EXPR _p "${_at} + 8")
            _le_uint("${_head}" ${_p} 2 _format)
            _le_uint("${_head}" ${_p}+2 2 _channels)
            _le_uint("${_head}" ${_p}+4 4 _rate)
            _le_uint("${_head}" ${_p}+14 2 _bits)
            if(_format EQUAL 65534 AND _chunk_size GREATER_EQUAL 40) # WAVE_FORMAT_EXTENSIBLE: sub-format
                _le_uint("${_head}" ${_p}+24 2 _format)
            endif()
            set(_found TRUE)
            break()
        endif()
        math(EXPR _at "${_at} + 8 + ${_chunk_size} + (${_chunk_size} & 1)")
    endwhile()
    if(NOT _found)
        message(FATAL_ERROR "${_name}: no fmt chunk in the first 4 KB (not a WAV file?)")
    endif()
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
    _le_uint("${_head}" 26 1 _segments)
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
    _le_uint("${_head}" ${_p}+11 1 _channels)
    _le_uint("${_head}" ${_p}+12 4 _rate)
    _check_layout("${_name}" ${_channels} ${_rate})
else()
    message(FATAL_ERROR "nnh_check_sound: ${_name} is not .wav or .ogg")
endif()
