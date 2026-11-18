# nnh_link — the library nnh serve (PC) and the NNH Link app (3DS, and the simulator) share (spec 011,
# decision D24): sockets, SHA-256/HMAC, the message codec, pairing and authentication. Built in every
# preset; its tests run on PC. Explicit source lists.

add_library(nnh_link STATIC
    src/address.cpp
    src/discovery.cpp
    src/frame.cpp
    src/handshake.cpp
    src/messages.cpp
    src/net.cpp
    src/sha256.cpp
    src/wire.cpp
)
target_include_directories(nnh_link PUBLIC include)
nnh_engine_options(nnh_link)
if(WIN32)
    target_link_libraries(nnh_link PUBLIC ws2_32 iphlpapi)
endif()

if(NNH_BUILD_TESTS)
    # Fuzz target bodies: run as a mini-fuzz in the tests and as libFuzzer executables below.
    add_library(nnh_link_fuzz_targets STATIC tests/fuzz/link_targets.cpp)
    target_include_directories(nnh_link_fuzz_targets PUBLIC tests)
    target_link_libraries(nnh_link_fuzz_targets PUBLIC nnh_link)
    nnh_engine_options(nnh_link_fuzz_targets)

    add_executable(nnh_link_tests
        tests/test_address.cpp
        tests/test_fuzz_link.cpp
        tests/test_handshake.cpp
        tests/test_messages.cpp
        tests/test_net.cpp
        tests/test_sha256.cpp
    )
    target_link_libraries(nnh_link_tests PRIVATE nnh_link nnh_link_fuzz_targets nnh_test_framework)
    nnh_engine_options(nnh_link_tests)
    find_package(Threads REQUIRED) # the loopback tests run fakes on threads
    target_link_libraries(nnh_link_tests PRIVATE Threads::Threads)
    if(MINGW)
        target_link_options(nnh_link_tests PRIVATE -static) # see tools/nnh/CMakeLists.txt
    endif()
    add_test(NAME nnh_link COMMAND nnh_link_tests)

    # libFuzzer executables (clang only), as in tests/CMakeLists.txt: -DNNH_BUILD_FUZZERS=ON, then e.g.
    # build/pc-debug/link/fuzz_link_frames -max_total_time=60
    if(NNH_BUILD_FUZZERS)
        foreach(lib nnh_link nnh_link_fuzz_targets)
            target_compile_options(${lib} PRIVATE -fsanitize=fuzzer-no-link,address)
        endforeach()
        foreach(name messages frames discovery)
            add_executable(fuzz_link_${name} tests/fuzz/libfuzzer_link_${name}.cpp)
            target_link_libraries(fuzz_link_${name} PRIVATE nnh_link_fuzz_targets)
            target_compile_options(fuzz_link_${name} PRIVATE -fsanitize=fuzzer,address)
            target_link_options(fuzz_link_${name} PRIVATE -fsanitize=fuzzer,address)
        endforeach()
    endif()
endif()
