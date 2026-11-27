# nnh::core's static library (spec 032 FR-19, D55 item 9): the archive of the object library nnh_core_objects
# (engine/core/CMakeLists.txt), in a folder of its own so that only its archive step uses a response file.
#
# On Windows, devkitPro's MSYS2 CMake assumes the POSIX command-line limit and writes the archive command with every
# object on it: 447 objects made a 27,354-character command in the repository and 35,454 in a game project
# (_deps/nnhge-build/), against Windows' 32,767. CMAKE_NINJA_FORCE_RESPONSE_FILE is read per directory, so setting it
# here puts this one archive's object list in a response file. CMake reads that variable but does not document it
# (`cmake --help-variable CMAKE_NINJA_FORCE_RESPONSE_FILE`: not defined), so a CMake update could drop it without a
# word: scripts/check_command_length.sh guards it (stage ends, scripts/vm_check.sh final, the release checklist). Its paths are relative to the build folder, which the
# native arm-none-eabi-gcc-ar reads. (Set for the whole build it breaks every compile: the compilers' response files
# would hold /e/... include paths the native compiler can't read.) PC presets: unchanged (native CMake already uses
# response files where needed on Windows).
#
# Same archive as before the split: engine/core/libnnh_core.a with the same members (the two vendored objects,
# stb_vorbis.o and f2s.o, now last instead of first) and the same usage requirements (games and platform libraries see
# no difference; hello.3dsx stayed byte-identical).

if(NNH_PLATFORM STREQUAL "3ds")
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON)
endif()

add_library(nnh_core STATIC $<TARGET_OBJECTS:nnh_core_objects>)
add_library(nnh::core ALIAS nnh_core)
set_target_properties(nnh_core PROPERTIES
    LINKER_LANGUAGE CXX
    ARCHIVE_OUTPUT_DIRECTORY "${_nnh_core_binary_dir}")
target_include_directories(nnh_core PUBLIC ${_nnh_core_include_dir})
target_link_libraries(nnh_core PRIVATE nnh::core_contract nnh_stb_vorbis nnh_ryu)
target_compile_features(nnh_core PUBLIC cxx_std_20)
target_compile_definitions(nnh_core PUBLIC ${_nnh_flavor_definitions})
