# nnh::platform_fake — a scriptable, recording platform for tests. See README.md.

add_library(nnh_platform_fake STATIC
    src/app_fake.cpp
    src/control_fake.cpp
    src/crash_fake.cpp
    src/input_fake.cpp
    src/log_fake.cpp
    src/time_fake.cpp
    src/video_fake.cpp
)
add_library(nnh::platform_fake ALIAS nnh_platform_fake)
target_include_directories(nnh_platform_fake PUBLIC include PRIVATE src)
target_link_libraries(nnh_platform_fake PUBLIC nnh::core_contract)
nnh_engine_options(nnh_platform_fake)
