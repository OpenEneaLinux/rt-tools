find_program (POD2MAN pod2man)

if (POD2MAN)
  ADD_CUSTOM_TARGET(BitcalcManPage ALL)

  ADD_CUSTOM_COMMAND(
    TARGET BitcalcManPage
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/../src/bitcalc.c
    COMMAND ${POD2MAN} ARGS --release="Enea Linux RT-Tools Suite" --center="User Commands" --name=BITCALC ${CMAKE_CURRENT_SOURCE_DIR}/../src/bitcalc.c ${CMAKE_CURRENT_BINARY_DIR}/bitcalc.1
    OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/bitcalc.1
    COMMENT "Building man page for bitcalc"
  )

  install (FILES ${CMAKE_CURRENT_BINARY_DIR}/bitcalc.1 DESTINATION share/man/man1)
else ()
  message (WARNING "pod2man: Command not found, not building man pages")
endif ()
