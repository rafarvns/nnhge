# ctest helper: cmake -DUNIQUE_ID=... -DPRODUCT_CODE=... -DVERSION=... -DTITLE=... -DDESCRIPTION=...
#   -DAUTHOR=... -P check_package.cmake — runs the metadata checks nnh_add_game() runs at configure.
cmake_minimum_required(VERSION 3.25)
include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/nnh_package_rules.cmake)
nnh_check_package_values(UNIQUE_ID "${UNIQUE_ID}" PRODUCT_CODE "${PRODUCT_CODE}" VERSION "${VERSION}"
                         TITLE "${TITLE}" DESCRIPTION "${DESCRIPTION}" AUTHOR "${AUTHOR}")
