# Add sanitizer
# NOTE: Disabled since this requires too recent version of the compiler for
#       Yocto builds. Those using newer version of the compiler may enable
#       this.
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")

# If check unit test framework is found
check_optional_module(CHECK check)
if (CHECK_FOUND)
  add_definitions (${CHECK_CFLAGS})
  string (REPLACE ";" " " CHECK_LDFLAGS_OTHER_STR "${CHECK_LDFLAGS_OTHER}")
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CHECK_LDFLAGS_OTHER_STR}")
  message(STATUS "Building unit tests")
else (CHECK_FOUND)
  message (STATUS "Check library not found, unit tests not built")
endif (CHECK_FOUND)

if (LTTNG_UST_FOUND)
  add_definitions(-DHAVE_LTTNG)
  message(STATUS "lttng-ust detected, tracing enabled")
else ()
  message(STATUS "lttng-ust NOT detected, tracing disabled")
endif (LTTNG_UST_FOUND)

find_program(LCOV lcov)
if (LCOV)
  # Add profile support
  message (STATUS "Collecting code coverage information")
  add_definitions(-g --coverage)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs")
  set (LCOV_COLLECT "&& ${CMAKE_CURRENT_SOURCE_DIR}/../lcov_collect.sh")
else ()
  message (WARNING "lcov not found: Not collecting code coverage information")
  set (LCOV_COLLECT "")
endif ()

macro (build_test name)
  if (CHECK_FOUND)
    target_link_libraries(${name} ${CHECK_LIBRARIES})
  endif ()
  if (LTTNG_UST_FOUND)
    target_link_libraries(${name} ${LTTNG_UST_LIBRARIES})
  endif ()
endmacro (build_test)

# Call syntax:
#   do_test
macro (do_test test_name command)
  add_test (${test_name} sh -c "(${command}) ${LCOV_COLLECT}")
  set_tests_properties (${test_name} PROPERTIES TIMEOUT "20")
endmacro (do_test)

macro (do_test_regex test_name command result)
  do_test(${test_name} ${command})
  set_tests_properties (${test_name} PROPERTIES PASS_REGULAR_EXPRESSION ${result})
endmacro (do_test_regex)

macro (do_fail_test_regex test_name command result)
  do_test(${test_name} ${command})
  set_tests_properties (${test_name} PROPERTIES WILL_FAIL true FAIL_REGULAR_EXPRESSION ${result})
endmacro (do_fail_test_regex)

#
# Unit testing
#

if (CHECK_FOUND)
  add_executable(test_bitmap test_bitmap.c)
  build_test (test_bitmap)
  do_test (test_bitmap ./test_bitmap)
endif ()

#
# Functional testing
#

add_executable(test_bitcalc ../src/bitcalc.c ../src/bitmap.c ../src/common.c)
build_test (test_bitcalc)

do_test_regex (bitcalc_help "./test_bitcalc --help" "Usage:")
do_test_regex (bitcalc_h "./test_bitcalc -h" "Usage:")
do_test_regex (bitcalc_version "./test_bitcalc --version" "bitcalc ${bitcalc_MAJOR_VER}.${bitcalc_MINOR_VER}")
do_test_regex (bitcalc_V "./test_bitcalc -V" "bitcalc ${bitcalc_MAJOR_VER}.${bitcalc_MINOR_VER}")
do_test_regex (bitcalc_example1 "./test_bitcalc '#1-2,4-5 #2-4 xor'" "2a")
do_test_regex (bitcalc_all_types "./test_bitcalc '0x4 #0-1,3-4 ffff,fffffffc and xor'" "00000000001c")
do_test_regex (bitcalc_example_stdin "echo '#1-2,5-4 #2-4,6 xor' | ./test_bitcalc '&2' --file=- '&3'" "7 6a 3")
do_test_regex (bitcalc_format_list_long "./test_bitcalc --format=list 0xfe" "1-7")
do_test_regex (bitcalc_format_list_short "./test_bitcalc -Flist 0xfe" "1-7")
do_test_regex (bitcalc_format_u32list_long "./test_bitcalc --format=u32list 0xffffffff88888888" "ffffffff,88888888")
do_test_regex (bitcalc_format_u32list_short "./test_bitcalc -vvv -Fu32list 0xffffffff88888888" "ffffffff,88888888")
do_test_regex (bitcalc_format_array "./test_bitcalc --format=array '#1-3,5,1024'" "1 2 3 5 1024")

# Negative tests

do_fail_test_regex (bitcalc_verbose_illegal_list "./test_bitcalc -vvv '#qwerty'" "Error while parsing list")
