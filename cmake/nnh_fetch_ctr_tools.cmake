# cmake -DHOST=<windows|linux> -DDIR=<tools dir> -P nnh_fetch_ctr_tools.cmake
#
# Build step (target nnh_ctr_tools, spec 009): downloads makerom and bannertool once into DIR, pinned by
# SHA-256 (constitution X), and leaves them at DIR/makerom[.exe] and DIR/bannertool[.exe]. Runs only when a
# .cia is built, so configuring or building games never downloads anything.
#   makerom v0.19.0     github.com/3DSGuy/Project_CTR           MIT
#   bannertool 1.2.3    github.com/carstene1ns/3ds-bannertool   MIT (the maintained fork)

cmake_minimum_required(VERSION 3.25) # -P scripts don't inherit the project's policies

set(makerom_url "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0")
set(banner_url "https://github.com/carstene1ns/3ds-bannertool/releases/download/1.2.3")
if(HOST STREQUAL "windows")
    set(makerom_pkg "makerom-v0.19.0-win_x86_64.zip")
    set(makerom_sha "88df4455e60556374e202d507f54ee03fac7493ec9554ab853157524b6d69db0")
    set(banner_pkg "bannertool-1.2.3-windows.zip")
    set(banner_sha "d0f277bd875e2ad411844378155d27afe124780fcccbd81d9a594e886e2dd3c4")
    set(ext ".exe")
elseif(HOST STREQUAL "linux")
    set(makerom_pkg "makerom-v0.19.0-ubuntu_x86_64.zip")
    set(makerom_sha "287b809dec064e0ad597e3d272c49ecb7eed41693d5ee6fef9d8a8aa24c2497e")
    set(banner_pkg "bannertool-1.2.3-linux.tar.gz")
    set(banner_sha "748519d200519db18e9fd00c332ac32f5411e41230258291e89953a76c1f7155")
    set(ext "")
else()
    message(FATAL_ERROR "nnh_fetch_ctr_tools: HOST must be windows or linux")
endif()

# Downloads and extracts one tool, then moves its program to DIR/<name><ext>.
function(fetch name url sha256)
    set(out "${DIR}/${name}${ext}")
    if(EXISTS "${out}")
        return()
    endif()
    get_filename_component(archive_name "${url}" NAME)
    set(work "${DIR}/${name}.download")
    file(REMOVE_RECURSE "${work}")
    message(STATUS "NNHGE: downloading ${name} (${archive_name})")
    file(DOWNLOAD "${url}" "${work}/${archive_name}" EXPECTED_HASH SHA256=${sha256} STATUS status TLS_VERIFY ON)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        list(GET status 1 reason)
        message(FATAL_ERROR "NNHGE: could not download ${name} (${reason}). Without network, configure with "
                            "-DNNH_MAKEROM=<makerom> -DNNH_BANNERTOOL=<bannertool> (docs/guides/packaging.md)")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${work}/${archive_name}" DESTINATION "${work}/x")
    file(GLOB_RECURSE found "${work}/x/*${name}${ext}")
    if(NOT found)
        message(FATAL_ERROR "NNHGE: ${name}${ext} not found in ${archive_name}")
    endif()
    list(GET found 0 program)
    file(COPY_FILE "${program}" "${out}")
    file(CHMOD "${out}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    file(REMOVE_RECURSE "${work}")
endfunction()

file(MAKE_DIRECTORY "${DIR}")
fetch(makerom "${makerom_url}/${makerom_pkg}" ${makerom_sha})
fetch(bannertool "${banner_url}/${banner_pkg}" ${banner_sha})
