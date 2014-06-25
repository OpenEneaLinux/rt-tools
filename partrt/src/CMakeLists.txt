add_executable(partrt partrt.c partrt_common.c partrt_create.c partrt_cpumask.c partrt_cpuset.c)

if (LTTNG_UST_FOUND)
  target_link_libraries(partrt ${LTTNG_UST_LIBRARIES})
  add_definitions(-DHAVE_LTTNG)
  message(STATUS "lttng-ust detected, tracing enabled")
else ()
  message(STATUS "lttng-ust NOT detected, tracing disabled")
endif (LTTNG_UST_FOUND)

# add the install targets
install (TARGETS partrt DESTINATION bin)
