cmake_minimum_required (VERSION 2.6)

# Custom variants of package finders
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/Modules/")

# Create bitcalc project, which is a binary
project (bitcalc)

# Create "test" build target, used by test subdirectory
enable_testing()
set(CTEST_OUTPUT_ON_FAILURE ON)

# The version number for bitcalc tool
set (bitcalc_VERSION_MAJOR 1)
set (bitcalc_VERSION_MINOR 0)

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

# Add version number as macros when compiling
add_definitions(-Dbitcalc_VERSION_MAJOR=${bitcalc_VERSION_MAJOR} -Dbitcalc_VERSION_MINOR=${bitcalc_VERSION_MINOR})

#####################
# Add a few warnings
#


# The safe warnings
add_definitions(-Wall)

# The pretty safe warnings
add_definitions(-Wextra)

# Make sure warnings aren't accepted
add_definitions(-Werror)

# No variable name shadowing
add_definitions(-Wshadow)

# Catch the most common "use before initialized"
add_definitions(-Wuninitialized)

# Initializing with value dependent on itself
add_definitions(-Winit-self)

# Public functions need prototypes
add_definitions(-Wmissing-prototypes)

# Check for unsafe printf/scanf usage
add_definitions(-Wformat-security)

# Parameters not used, use "(void) param" if this is intentional
add_definitions(-Wunused-parameter)

# Check for missing __attribute__
add_definitions(-Wsuggest-attribute=pure)
add_definitions(-Wsuggest-attribute=const)
add_definitions(-Wsuggest-attribute=noreturn)

# Warn for undefined identifier in #if
add_definitions(-Wundef)

# Warn for anything depending on sizeof (void) or function
add_definitions(-Wpointer-arith)

# Warn for casts to non-matching type
add_definitions(-Wbad-function-cast)

# Warn when pointer is cast to remove type qualifier from target type
add_definitions(-Wcast-qual)

# Warn when a pointer cast increases alignment requirement
add_definitions(-Wcast-align)

# Make sure string constants have the const type
add_definitions(-Wwrite-strings)

# Warn for implicit conversion that can change the value
add_definitions(-Wconversion)

# Warn for a "goto" or similar the misses an initialization of something that
# is being used
add_definitions(-Wjump-misses-init)

# Warn about suspicious use of logical operators in expressions
add_definitions(-Wlogical-op)

# Warn if in a loop with constant number of iterations the compiler detects
# undefined behavior in some statement during one or more of the iterations
add_definitions(-Wno-aggressive-loop-optimizations)

# Warn if a function is declared or defined without specifying the argument
# types
add_definitions(-Wstrict-prototypes)

# Warn if a global function is defined without a previous declaration
add_definitions(-Wmissing-declarations)

# Warn if anything is declared more than once in the same scope, even in cases
# where multiple declaration is valid and changes nothing
add_definitions(-Wredundant-decls)

# Protect against stack smashing, this is default for some GCC builds but make
# sure we have it
add_definitions(-fstack-protector)


#####################
# Add sub-directories


# Application source directory
add_subdirectory (src)

# Unit test directory
add_subdirectory (test)

# Build man page
add_subdirectory (man)
