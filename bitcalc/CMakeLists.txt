cmake_minimum_required (VERSION 2.6)

# Custom variants of package finders
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/Modules/")

# Find pkg-config tool to be used when searching for check library
# See http://www.cmake.org/cmake/help/v2.8.2/cmake.html#module:FindPkgConfig
find_package(PkgConfig)

# Macro for printing value of a variable name
macro (printvar name)
  message(STATUS "${name}: ${${name}}")
endmacro (printvar)

# Macro for finding an optional module (library), and add it to the build.
# Use <var>_FOUND to determine whether this succeeded or not.
macro (check_optional_module var name)
  pkg_check_modules(${var} ${name})

  printvar (${var}_LIBRARIES)
  printvar (${var}_LIBRARY_DIRS)
  printvar (${var}_LDFLAGS)
  printvar (${var}_LDFLAGS_OTHER)
  printvar (${var}_INCLUDE_DIRS)
  printvar (${var}_CFLAGS)
  printvar (${var}_CFLAGS_OTHER)

  # If check unit test framework is found
  if (${var}_FOUND)
    # Handle ${var}_INCLUDE_DIRS
    include_directories(${${var}_INCLUDE_DIRS})

    # Handle ${var}_LIBRARY_DIRS
    link_directories(${${var}_LIBRARY_DIRS})

    # Handle ${var}_CFLAGS
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${var}_CFLAGS}")

    # Handle ${var}_LDFLAGS_OTHER, ${var}_LDFLAGS is handled indirectly by
    # ${var}_LIBRARY_DIRS and ${var}_LIBRARIES
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${${var}_LDFLAGS_OTHER}")

    # Handle ${var}_LIBRARIES
    set(LIBS "${LIBS} ${${var}_LIBRARIES}")

  endif (${var}_FOUND)
endmacro (check_optional_module)

check_optional_module(LTTNG_UST lttng-ust)

# Common flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -Wshadow -Wuninitialized -Winit-self -Wmissing-prototypes -Wformat-security -Wunused-parameter -Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion -Wjump-misses-init -Wlogical-op -Wstrict-prototypes -Wmissing-declarations -Wredundant-decls -fstack-protector -Dbitcalc_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} -Dbitcalc_VERSION_MINOR=${PROJECT_VERSION_MINOR}")

# Application source directory
add_subdirectory (src)

# Unit test directory
add_subdirectory (test)

# Build man page
add_subdirectory (man)
