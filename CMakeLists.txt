cmake_minimum_required(VERSION 3.2)
cmake_policy(SET CMP0037 NEW)
PROJECT(OSREC-interp)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_modules/")

add_executable(OSRECinterp.bin OSREC-interp.cpp tinyfiledialogs.c)
target_link_libraries(OSRECinterp.bin )
