cmake_minimum_required (VERSION 2.6)

# Create partrt project, which is a shell script
project (partrt)

install (PROGRAMS partrt DESTINATION bin)

# Create "test" build target, used by test subdirectory
enable_testing()
set(CTEST_OUTPUT_ON_FAILURE ON)

# The version number for partrt tool
set (partrt_VERSION_MAJOR 1)
set (partrt_VERSION_MINOR 0)

# Find pkg-config tool to be used when searching for check library
# See http://www.cmake.org/cmake/help/v2.8.2/cmake.html#module:FindPkgConfig
find_package(PkgConfig)

# Macro for printing value of a variable name
macro (printvar name)
  message(STATUS "${name}: ${${name}}")
endmacro (printvar)

