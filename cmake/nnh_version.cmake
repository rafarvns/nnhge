# The engine's version for CMake (spec 032 FR-10, D55 item 4): NNHGE_VERSION and NNHGE_VERSION_MAJOR / _MINOR / _PATCH,
# from project(nnhge VERSION ...), as INTERNAL cache entries rewritten at every configure, so every folder of a game
# project sees them (also when the game adds the engine from inside a function):
#   if(NNHGE_VERSION VERSION_LESS 1.1)
#       message(FATAL_ERROR "mygame needs NNHGE 1.1 or later (found ${NNHGE_VERSION})")
#   endif()
# Included once by the engine's top-level CMakeLists.txt, right after project(). The release label and the C++
# counterpart are spec 034's (this file is where the label goes).

set(NNHGE_VERSION "${PROJECT_VERSION}" CACHE INTERNAL "NNHGE version")
set(NNHGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}" CACHE INTERNAL "NNHGE major version")
set(NNHGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}" CACHE INTERNAL "NNHGE minor version")
set(NNHGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}" CACHE INTERNAL "NNHGE patch version")
message(STATUS "NNHGE ${NNHGE_VERSION}: ${PROJECT_SOURCE_DIR}")
