############################################
#
# How to build bsdcpio
#
############################################
IF(ENABLE_CPIO)
  SET(bsdcpio_SOURCES
    cmdline.c
    cpio.c
    cpio.h
    cpio_platform.h
    err.c
    matching.c
    matching.h
    pathmatch.c
    pathmatch.h
  )
  IF(WIN32 AND NOT CYGWIN)
    LIST(APPEND bsdcpio_SOURCES cpio_windows.c)
    LIST(APPEND bsdcpio_SOURCES cpio_windows.h)
  ENDIF(WIN32 AND NOT CYGWIN)

  SET(bsdcpio_MANS bsdcpio.1)

  #
  # Register target
  #
  ADD_EXECUTABLE(bsdcpio ${bsdcpio_SOURCES})
  IF(ENABLE_CPIO_SHARED)
    TARGET_LINK_LIBRARIES(bsdcpio archive ${ADDITIONAL_LIBS})
  ELSE(ENABLE_CPIO_SHARED)
    TARGET_LINK_LIBRARIES(bsdcpio archive_static ${ADDITIONAL_LIBS})
  ENDIF(ENABLE_CPIO_SHARED)
  IF(WIN32 AND NOT CYGWIN)
    SET_TARGET_PROPERTIES(bsdcpio PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    SET(BSDCPIO ${CMAKE_BINARY_DIR}/debug/bsdcpio.exe)
  ELSE(WIN32 AND NOT CYGWIN)
    SET(BSDCPIO ${CMAKE_CURRENT_BINARY_DIR}/bsdcpio)
  ENDIF(WIN32 AND NOT CYGWIN)
  #
  INSTALL(TARGETS bsdcpio RUNTIME DESTINATION bin)
  INSTALL_MAN(${bsdcpio_MANS})
ENDIF(ENABLE_CPIO)

add_subdirectory(test)
