# One case of nnh_pin_check (check.cmake): cmake -DURL=... -DSHA=... -P one.cmake
include("${CMAKE_CURRENT_LIST_DIR}/../../nnh_release_pin.cmake")
nnh_check_release_pin("${URL}" "${SHA}")
