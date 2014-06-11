find_package(Check)

if (LIBCHECK_FOUND)
# Path to config.h
include_directories("${PROJECT_BINARY_DIR}")

# Check unit test include path
include_directories(${LIBCHECK_INCLUDE_DIRS})

# Add compiler flags needed by check
add_definitions(-pthread ${LIBCHECK_DEFINITIONS})

message(STATUS "Building unit tests")

macro (define_test name)
      project (${name})
      add_executable(${name} ${name}.c ../src/partrt_common.c)
      target_link_libraries(${name} ${LIBCHECK_LIBRARIES} -lm -lpthread -lrt)
      install (TARGETS ${name} DESTINATION bin)
      add_test (${name} ${name})
endmacro (define_test)

define_test (test_partrt_cpumask)

else (LIBCHECK_FOUND)
message (STATUS "Unit tests not built")
endif (LIBCHECK_FOUND)
