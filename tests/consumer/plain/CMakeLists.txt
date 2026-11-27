# A library of the game project's own, not a game (spec 032 FR-8): compiled here and in
# tests/consumer_tools/plain_only/ (the same folder without the engine); the two compile commands must match.
add_library(consumer_plain STATIC plain.cpp)
target_compile_options(consumer_plain PRIVATE -Wshadow)
target_compile_definitions(consumer_plain PRIVATE CONSUMER_PLAIN=1)
