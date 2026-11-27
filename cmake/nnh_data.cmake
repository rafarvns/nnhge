# Data assets' build side (spec 014 FR-3, FR-15, decision D29). Called by nnh_add_game(... DATA_SCHEMA ...).
#
#   nnh_add_data_schema(<target> <schema sources...>)
#       Compiles the schema sources (the files holding NNH_DATA_TYPE / STRUCT / ENUM tables) once for <target>
#       (an OBJECT library <target>_data_schema_objs whose objects are added to <target>) and builds the host
#       program <target>_data_schema from the same declarations, which writes the schema description
#       <binary dir>/nnh_data/<target>.nnhschema that nnh-assetc reads (nnh_assets.cmake: data and index steps).
#         - PC presets: the program links those very objects with nnh_data_schema_support (tools/data_schema):
#           nothing is compiled twice.
#         - cross presets (3DS): the objects are console code, so the program is an ExternalProject compiling
#           tools/data_schema with the schema sources for the host (NNH_HOST_CXX, like nnh-assetc). It has no
#           BUILD_ALWAYS: its build step depends on the console objects (through a stamp) and on the program's own
#           engine sources, so it reruns only when a schema source, a header one includes, or the engine changes
#           (plan 014 "Stage 0 refinements").
#       The program rewrites the description only when its text changes (restat), so a schema edit that doesn't
#       change the schema reconverts nothing. Schema sources must compile for the host and must not call engine or
#       game functions (they are not linked against the engine there): declarations and tables only.
#       Include directories of the schema sources: the engine's public headers, the folder of the CMakeLists.txt
#       calling nnh_add_game and each schema source's folder.
#
#   nnh_use_data_schema(<target> <description file> <target producing it>)
#       Marks <target> as a game with data assets read with that description (nnh_add_assets reads the target
#       properties NNH_DATA_SCHEMA_FILE and NNH_DATA_SCHEMA_TARGET). Used by nnh_add_data_schema, and by tests
#       whose test "games" reuse the tests' schema.
#
# A game without DATA_SCHEMA calls neither: no host program, no data step, no index (FR-15).

include_guard(GLOBAL)
include(ExternalProject)
include("${CMAKE_CURRENT_LIST_DIR}/nnh_platform.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/nnh_game_defaults.cmake")

function(nnh_use_data_schema target description producer)
    set_target_properties(${target} PROPERTIES NNH_DATA_SCHEMA_FILE "${description}"
                                               NNH_DATA_SCHEMA_TARGET "${producer}")
endfunction()

# The schema program for the host in a cross preset; sets <out_program> to its path, or "" with <out_why> set.
function(_nnh_data_host_program target objs sources includes out_program out_why)
    _nnh_host_cxx(cxx) # nnh_host_tools.cmake
    if(NOT cxx)
        set(${out_program} "" PARENT_SCOPE)
        set(${out_why} "no host C++ compiler for the data schema program: set NNH_HOST_CXX (environment or -D)"
            PARENT_SCOPE)
        return()
    endif()
    # The engine's root from this file, never from the calling folder (spec 032 FR-6); the program's own engine sources
    # (besides the schema sources): a change rebuilds it.
    set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    set(host_sources
        ${root}/tools/data_schema/CMakeLists.txt
        ${root}/tools/data_schema/schema_main.cpp
        ${root}/engine/core/src/core/crc32_update.cpp
        ${root}/engine/core/src/data/enum_names.cpp
        ${root}/engine/core/src/data/schema_check.cpp
        ${root}/engine/core/src/data/schema_fingerprint.cpp
        ${root}/engine/core/src/data/schema_registry.cpp
        ${root}/engine/core/src/data/schema_write.cpp)
    set(dir "${CMAKE_CURRENT_BINARY_DIR}/nnh_data/${target}_host")
    set(name "${target}_data_schema")
    if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows|MSYS|CYGWIN|MINGW")
        set(exe "${dir}/${name}.exe")
    else()
        set(exe "${dir}/${name}")
    endif()
    string(REPLACE ";" "|" sources_arg "${sources}")
    string(REPLACE ";" "|" includes_arg "${includes}")
    ExternalProject_Add(${name}
        SOURCE_DIR ${root}/tools/data_schema
        BINARY_DIR ${dir}
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/nnh_data/${target}_host-ep
        CMAKE_ARGS -DCMAKE_CXX_COMPILER=${cxx} -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                   -DNNH_SCHEMA_PROGRAM=${name} "-DNNH_SCHEMA_SOURCES=${sources_arg}"
                   "-DNNH_SCHEMA_INCLUDES=${includes_arg}"
        BUILD_BYPRODUCTS ${exe}
        INSTALL_COMMAND ""
        EXCLUDE_FROM_ALL TRUE)
    # Rebuilt when the console objects of the schema sources change (Ninja rebuilds those exactly when a schema
    # source or a header it includes changes) or when the program's own sources do — never on every build.
    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/nnh_data/${target}_objects.stamp")
    add_custom_command(OUTPUT "${stamp}"
        COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
        DEPENDS $<TARGET_OBJECTS:${objs}> ${host_sources}
        VERBATIM)
    ExternalProject_Add_StepDependencies(${name} build "${stamp}")
    set(${out_program} "${exe}" PARENT_SCOPE)
endfunction()

function(nnh_add_data_schema target)
    nnh_platform(platform "nnh_add_game(${target})")
    set(sources "")
    set(includes "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach(source IN LISTS ARGN)
        get_filename_component(path "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        get_filename_component(dir "${path}" DIRECTORY)
        list(APPEND sources "${path}")
        list(APPEND includes "${dir}")
    endforeach()
    list(REMOVE_DUPLICATES includes)
    if(NOT sources)
        message(FATAL_ERROR "nnh_add_game(${target}): DATA_SCHEMA needs at least one source")
    endif()

    # Compiled like the game's own sources (the engine's usage requirements), added to it as objects: never through
    # a static archive, so the linker can't drop the registrations.
    set(objs ${target}_data_schema_objs)
    add_library(${objs} OBJECT ${sources})
    target_link_libraries(${objs} PRIVATE nnh::core)
    target_include_directories(${objs} PRIVATE ${includes})
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:${objs}>)
    _nnh_game_target_defaults(${objs}) # the game's build settings (spec 032 FR-7)

    set(description "${CMAKE_CURRENT_BINARY_DIR}/nnh_data/${target}.nnhschema")
    if(platform STREQUAL "pc")
        set(program_target ${target}_data_schema)
        add_executable(${program_target} $<TARGET_OBJECTS:${objs}>)
        _nnh_game_target_defaults(${program_target})
        set_target_properties(${program_target} PROPERTIES LINKER_LANGUAGE CXX)
        target_link_libraries(${program_target} PRIVATE nnh_data_schema_support)
        if(CMAKE_CXX_PLATFORM_ID STREQUAL "MinGW")
            target_link_options(${program_target} PRIVATE -static)
        endif()
        set(command $<TARGET_FILE:${program_target}> "${description}")
        set(depends ${program_target})
    else()
        _nnh_data_host_program(${target} ${objs} "${sources}" "${includes}" program why)
        if(program)
            set(command "${program}" "${description}")
            set(depends "${program};${target}_data_schema")
        else()
            message(STATUS "NNHGE ${target}: ${why}")
            set(command ${CMAKE_COMMAND} -E echo "${target}: ${why} (docs/tech/cmake.md)"
                COMMAND ${CMAKE_COMMAND} -E false)
            set(depends "")
        endif()
    endif()
    add_custom_command(OUTPUT "${description}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/nnh_data"
        COMMAND ${command}
        DEPENDS ${depends}
        COMMENT "Data schema of ${target}"
        VERBATIM)
    add_custom_target(${target}_data_description DEPENDS "${description}")
    nnh_use_data_schema(${target} "${description}" ${target}_data_description)
endfunction()
