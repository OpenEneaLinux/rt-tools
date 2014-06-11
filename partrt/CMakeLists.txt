cmake_minimum_required (VERSION 2.6)

# Custom variants of package finders
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/Modules/")

# Create partrt project, which is a binary
project (partrt)

# Create "test" build target, used by test subdirectory
enable_testing()
set(CTEST_OUTPUT_ON_FAILURE ON)

# The version number for partrt tool
set (partrt_VERSION_MAJOR 1)
set (partrt_VERSION_MINOR 0)

# Add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
include_directories("${PROJECT_BINARY_DIR}")

# Create config.h file based on CMake variables and config.h.in
configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )

# Application source directory
add_subdirectory (src)

# Unit test directory
add_subdirectory (test)
