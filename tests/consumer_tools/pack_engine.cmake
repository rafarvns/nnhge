# cmake -DENGINE=<engine dir> -DWORK=<dir> -P pack_engine.cmake
# Packs the engine folder as <WORK>/nnhge.tar.gz (the layout of a source archive: no build/, no .git) and writes
# <WORK>/engine_pin.cmake with its file:// URL and SHA-256, the pin the consumer project reads (copy_project.sh).
cmake_minimum_required(VERSION 3.25)
include("${CMAKE_CURRENT_LIST_DIR}/consumer_engine.cmake")
get_filename_component(WORK "${WORK}" ABSOLUTE)
file(MAKE_DIRECTORY "${WORK}")
consumer_pack_engine("${ENGINE}" "${WORK}" url sha)
file(WRITE "${WORK}/engine_pin.cmake"
     "# Written by tests/consumer_tools/copy_project.sh: the engine archive this copy pins.\n"
     "set(CONSUMER_ENGINE_URL \"${url}\")\nset(CONSUMER_ENGINE_SHA256 \"${sha}\")\n")
message(STATUS "pack_engine: ${url} SHA-256 ${sha}")
