# nnh_optional_part(<option> <folder>)
# Adds one of the engine's optional parts (examples, tests, apps, tools: spec 032 FR-4, FR-5, D55 item 2) when its
# option is ON. A part whose option is ON and whose folder is missing (a pruned copy of the engine) fails the configure,
# naming the option and the folder: a wrong copy is loud, never silently half-built.

include_guard(GLOBAL)

function(nnh_optional_part option folder)
    if(NOT ${option})
        return()
    endif()
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${folder}/CMakeLists.txt")
        message(FATAL_ERROR "NNHGE: ${option} is ON but the engine has no ${folder}/ folder "
                            "(${PROJECT_SOURCE_DIR}/${folder}): set ${option}=OFF or use a full engine copy")
    endif()
    add_subdirectory("${PROJECT_SOURCE_DIR}/${folder}" "${PROJECT_BINARY_DIR}/${folder}")
endfunction()
