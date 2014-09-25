check_optional_module(CHECK check)

if (LTTNG_UST_FOUND)
  add_definitions(-DHAVE_LTTNG)
  message(STATUS "lttng-ust detected, tracing enabled")
else ()
  message(STATUS "lttng-ust NOT detected, tracing disabled")
endif (LTTNG_UST_FOUND)

# Add profile support
add_definitions(-g --coverage)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

# If check unit test framework is found
if (CHECK_FOUND)
  message(STATUS "Building unit tests")

  macro (define_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} ${CHECK_LIBRARIES})
    if (LTTNG_UST_FOUND)
      target_link_libraries(${name} ${LTTNG_UST_LIBRARIES})
    endif (LTTNG_UST_FOUND)
    add_test (${name} ${name})
  endmacro (define_test)

  #
  # List of all test cases, one line per implementation file.
  #

  define_test (test_bitmap)

else (CHECK_FOUND)
  message (STATUS "Unit tests not built")
endif (CHECK_FOUND)

#
# Functional testing
#

macro (do_test test_name arg result)
  add_test (${test_name} ../src/bitcalc ${arg})
  set_tests_properties (${test_name}
    PROPERTIES PASS_REGULAR_EXPRESSION ${result})
endmacro (do_test)

do_test (bitcalc_help "--help" "Usage:")
do_test (bitcalc_h "-h" "Usage:")
do_test (bitcalc_version "--version" "bitcalc 1.0")
do_test (bitcalc_V "-V" "bitcalc 1.0")
do_test (bitcalc_example1 "#1-2,4-5 #2-4 xor" "2a")

add_test (bitcalc_example_stdin sh -c "echo '#1-2,4-5 #2-4 xor' | ../src/bitcalc --file=-")
set_tests_properties (bitcalc_example_stdin PROPERTIES PASS_REGULAR_EXPRESSION "2a")

add_test (bitcalc_all_types ../src/bitcalc "0x4 #0-1,3-4 %ffff,fffffffc and xor")
set_tests_properties (bitcalc_all_types PROPERTIES PASS_REGULAR_EXPRESSION "00000000001c")
add_test (bitcalc_format_list_long ../src/bitcalc --format=list 0xfe)
set_tests_properties (bitcalc_format_list_long PROPERTIES PASS_REGULAR_EXPRESSION "1-7")
add_test (bitcalc_format_list_short ../src/bitcalc -Flist 0xfe)
set_tests_properties (bitcalc_format_list_short PROPERTIES PASS_REGULAR_EXPRESSION "1-7")
add_test (bitcalc_format_u32list_long ../src/bitcalc --format=u32list 0xffffffff88888888)
set_tests_properties (bitcalc_format_u32list_long PROPERTIES PASS_REGULAR_EXPRESSION "ffffffff,88888888")
add_test (bitcalc_format_u32list_short ../src/bitcalc -Fu32list 0xffffffff88888888)
set_tests_properties (bitcalc_format_u32list_short PROPERTIES PASS_REGULAR_EXPRESSION "ffffffff,88888888")

