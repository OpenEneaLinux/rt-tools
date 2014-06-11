add_executable(partrt partrt.c partrt_common.c partrt_create.c partrt_cpumask.c)

# add the install targets
install (TARGETS partrt DESTINATION bin)
