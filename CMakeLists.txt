cmake_minimum_required (VERSION 2.6)

# Create "test" build target, used by test subdirectory
enable_testing()

# Bitcalc application
add_subdirectory (bitcalc)

# Partrt script
add_subdirectory (partrt)

# count_ticks script
add_subdirectory (count_ticks)

