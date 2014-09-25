find_program (POD2MAN pod2man)

if (POD2MAN_NOTFOUND)
  message (WARNING "pod2man: Command not found, not building man pages")
else ()
  ADD_CUSTOM_TARGET(BitcalcManPage ALL)

  ADD_CUSTOM_COMMAND(
    TARGET BitcalcManPage
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/../src/bitcalc.c
    COMMAND ${POD2MAN} ARGS --release="Enea Linux RT-Tools Suite" --center="User Commands" --name=BITCALC ${CMAKE_CURRENT_SOURCE_DIR}/../src/bitcalc.c ${CMAKE_CURRENT_BINARY_DIR}/bitcalc.1
    OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/bitcalc.1
  )

  ADD_CUSTOM_COMMAND(
    TARGET BitcalcManPage
    SOURCE BitcalcManPage
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/bitcalc.1
  )

  install (FILES bitcalc.1 DESTINATION share/man/man1)
endif ()
