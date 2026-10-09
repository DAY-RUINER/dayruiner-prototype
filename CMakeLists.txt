# DAY RUINER prototype — top-level build entry point.
# Configure from this directory, e.g.:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#
# The real build description lives in Source/CMakeLists.txt.
cmake_minimum_required(VERSION 3.22)

project(DayRuinerPrototype)

add_subdirectory(Source)
