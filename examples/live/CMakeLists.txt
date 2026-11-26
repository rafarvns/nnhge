nnh_add_game(live
    SOURCES main.cpp bench.cpp live_scene.cpp self_check.cpp sim.cpp tuning_handles.cpp
    TITLE "NNHGE Live values"
    DESCRIPTION "Live values: tunables, watches, pause and step"
    AUTHOR "NNHGE contributors")
# The world and the self-check: no fused multiply-add, so the sums are the same everywhere (spec 030 AC-2).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(sim.cpp self_check.cpp PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif()
