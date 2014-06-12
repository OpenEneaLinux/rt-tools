check_optional_module(CHECK check)

# If check unit test framework is found
if (CHECK_FOUND)
  # Path to config.h
  include_directories("${PROJECT_BINARY_DIR}")

  message(STATUS "Building unit tests")

  macro (define_test name)
    add_executable(${name} ${name}.c ../src/partrt_common.c)
    target_link_libraries(${name} ${CHECK_LIBRARIES})
    add_test (${name} ${name})
  endmacro (define_test)

  define_test (test_partrt_cpumask)

else (CHECK_FOUND)
  message (STATUS "Unit tests not built")
endif (CHECK_FOUND)
