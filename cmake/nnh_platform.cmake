# nnh_platform(<out_var> <who>)
# The system the engine builds for ("pc" or "3ds"), for the helpers a game project calls (spec 032 FR-6, D55 item 1).
# The engine's top-level CMakeLists.txt sets the global property NNH_PLATFORM once; a game project's folders don't
# have the engine's NNH_PLATFORM variable, so the helpers (nnh_add_game() and what it calls) read the property, never a
# variable of the caller's folder. An empty or unknown value fails the configure, naming <who> (for example
# "nnh_add_game(mygame)"), the value and the supported systems, before any target is created: no helper ever takes
# another system's branch silently. Tests set the property themselves (tests/consumer_tools/unknown_platform/).

include_guard(GLOBAL)

function(nnh_platform out_var who)
    get_property(value GLOBAL PROPERTY NNH_PLATFORM)
    if("${value}" STREQUAL "") # quoted: an unset property leaves `value` undefined
        message(FATAL_ERROR "${who}: unknown platform \"\" (empty, supported: pc, 3ds)")
    endif()
    if(NOT value MATCHES "^(pc|3ds)$") # literal: a variable of this file wouldn't exist in the caller's folder
        message(FATAL_ERROR "${who}: unknown platform \"${value}\" (supported: pc, 3ds)")
    endif()
    set(${out_var} "${value}" PARENT_SCOPE)
endfunction()
