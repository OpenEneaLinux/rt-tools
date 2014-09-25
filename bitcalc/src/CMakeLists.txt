add_executable(bitcalc bitcalc.c bitmap.c common.c)

if (LTTNG_UST_FOUND)
  target_link_libraries(bitcalc ${LTTNG_UST_LIBRARIES})
  add_definitions(-DHAVE_LTTNG)
  message(STATUS "lttng-ust detected, tracing enabled")
else ()
  message(STATUS "lttng-ust NOT detected, tracing disabled")
endif (LTTNG_UST_FOUND)

# add the install targets
install (TARGETS bitcalc DESTINATION bin)
