# _nnh_game_target_defaults(<target>)
# The repository's build settings on a target nnh_add_game() creates (spec 032 FR-7, FR-8, D55 item 3), so a game
# compiles the same inside and outside the engine repository:
#   - C++20, required, without compiler extensions: only when the calling folder has no CMAKE_CXX_STANDARD /
#     CMAKE_CXX_EXTENSIONS of its own (a game project that chose C++23 keeps it);
#   - the engine's compiler cache (NNH_CCACHE) as the compiler launcher: only when the calling folder has no launcher
#     of its own and NNH_CCACHE names a program (-DNNH_CCACHE= turns it off).
# Inside the engine repository the calling folder already has all of these, so nothing changes there. Only the targets
# nnh_add_game() creates get them: the game project's other targets are left alone (FR-8).

include_guard(GLOBAL)

function(_nnh_game_target_defaults target)
    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    endif()
    if(NOT DEFINED CMAKE_CXX_EXTENSIONS)
        set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    endif()
    if(NNH_CCACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
        set_target_properties(${target} PROPERTIES CXX_COMPILER_LAUNCHER "${NNH_CCACHE}")
    endif()
    if(NNH_CCACHE AND NOT CMAKE_C_COMPILER_LAUNCHER)
        set_target_properties(${target} PROPERTIES C_COMPILER_LAUNCHER "${NNH_CCACHE}")
    endif()
endfunction()
