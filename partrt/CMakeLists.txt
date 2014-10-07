cmake_minimum_required (VERSION 2.6)

# Create partrt project, which is a shell script
project (partrt)

install (PROGRAMS partrt DESTINATION bin)
install (FILES man/man1/partrt.1 DESTINATION share/man/man1)

