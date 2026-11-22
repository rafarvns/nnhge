# cmake -DLIST=<names file> -DOUT=<index file> [-DFOLDER=<assets output folder>] [-DGENERATED=<names>]
#       -P nnh_asset_index.cmake
#
# Writes the asset index core reads at init (spec 007): the asset paths of LIST, one per line, sorted.
# Fails the build, naming the file, if a name breaks the asset path rules (relative, '/'-separated, no
# "." or ".." or empty segments, letters/digits/space/-/_/., at most 255 bytes), is reserved (.nnh_assets, and
# .nnh_data — the data index, spec 014 — unless GENERATED names it: the build itself made it), or differs
# from another only by letter case (it would work on Windows and fail on the 3DS). With FOLDER, removes
# outputs of assets that no longer exist.

# A -P script doesn't inherit the project's policies: without this, CMake 3.x treats IN_LIST as an
# unknown argument (CMP0057 OLD); CMake 4 forces NEW, which hid the bug on Windows.
cmake_minimum_required(VERSION 3.25)

if(NOT LIST OR NOT OUT)
    message(FATAL_ERROR "nnh_asset_index: LIST and OUT are required")
endif()
# ENCODING UTF-8: without it file(STRINGS) silently drops non-ASCII bytes, so "ação.txt" would pass the
# check below as "ao.txt".
file(STRINGS "${LIST}" names ENCODING UTF-8)
list(REMOVE_ITEM names "")

set(lowered "")
foreach(name IN LISTS names)
    string(LENGTH "${name}" length)
    set(bad "")
    if(length GREATER 255)
        set(bad "longer than 255 characters")
    elseif(NOT name MATCHES "^[A-Za-z0-9 ._/-]+$")
        set(bad "use only letters, digits, space, '-', '_' and '.'")
    elseif(name MATCHES "(^|/)\\.\\.?(/|$)" OR name MATCHES "//" OR name MATCHES "^/" OR name MATCHES "/$")
        set(bad "no '.' or '..' folders and no empty folder names")
    elseif(name STREQUAL ".nnh_assets")
        set(bad "the name .nnh_assets is reserved")
    elseif(name STREQUAL ".nnh_data" AND NOT ".nnh_data" IN_LIST GENERATED)
        set(bad "the name .nnh_data is reserved (the data index of games with data assets)")
    endif()
    if(bad)
        message(FATAL_ERROR "asset \"${name}\": invalid name (${bad})")
    endif()
    string(TOLOWER "${name}" low)
    list(FIND lowered "${low}" clash)
    if(NOT clash EQUAL -1)
        list(GET names ${clash} other)
        message(FATAL_ERROR "assets \"${other}\" and \"${name}\" differ only by letter case: the 3DS treats them "
                            "as different files and Windows as the same one. Rename one of them.")
    endif()
    list(APPEND lowered "${low}")
endforeach()

list(SORT names)
string(REPLACE ";" "\n" text "${names}")
if(names)
    string(APPEND text "\n")
endif()
file(WRITE "${OUT}.new" "${text}")
file(COPY_FILE "${OUT}.new" "${OUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUT}.new")

if(FOLDER AND IS_DIRECTORY "${FOLDER}")
    file(GLOB_RECURSE existing LIST_DIRECTORIES false RELATIVE "${FOLDER}" "${FOLDER}/*")
    foreach(file IN LISTS existing)
        if(NOT file STREQUAL ".nnh_assets" AND NOT file IN_LIST names)
            file(REMOVE "${FOLDER}/${file}")
            message(STATUS "Removed stale asset ${file}")
        endif()
    endforeach()
endif()
