# The consumer project's own library without the engine (spec 032 FR-8, AC-7): its compile command here must equal the
# one in the consumer project, where the engine was added first.
cmake_minimum_required(VERSION 3.25)
project(plain_only LANGUAGES C CXX)
add_subdirectory(../../consumer/plain plain)
