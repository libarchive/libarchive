############################################
#
# How to build bsdtar
#
############################################
IF (ENABLE_TAR)
  SET(bsdtar_SOURCES
    bsdtar.c
    bsdtar.h
    bsdtar_platform.h
    cmdline.c
    getdate.c
    matching.c
    read.c
    siginfo.c
    subst.c
    tree.c
    tree.h
    util.c
    write.c
  )
  IF(WIN32 AND NOT CYGWIN)
    LIST(APPEND bsdtar_SOURCES bsdtar_windows.c)
    LIST(APPEND bsdtar_SOURCES bsdtar_windows.h)
    LIST(APPEND bsdtar_SOURCES ../libarchive/archive_windows.c)
  ENDIF(WIN32 AND NOT CYGWIN)

  SET(bsdtar_MANS bsdtar.1)

  #
  # Register target
  #
  ADD_EXECUTABLE(bsdtar ${bsdtar_SOURCES})
  IF(ENABLE_TAR_SHARED)
    TARGET_LINK_LIBRARIES(bsdtar archive ${ADDITIONAL_LIBS})
  ELSE(ENABLE_TAR_SHARED)
    TARGET_LINK_LIBRARIES(bsdtar archive_static ${ADDITIONAL_LIBS})
  ENDIF(ENABLE_TAR_SHARED)
  IF(WIN32 AND NOT CYGWIN)
    SET_TARGET_PROPERTIES(bsdtar PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    SET(BSDTAR ${CMAKE_BINARY_DIR}/debug/bsdtar.exe)
  ELSE(WIN32 AND NOT CYGWIN)
    SET(BSDTAR ${CMAKE_CURRENT_BINARY_DIR}/bsdtar)
  ENDIF(WIN32 AND NOT CYGWIN)
  #
  INSTALL(TARGETS bsdtar RUNTIME DESTINATION bin)
  INSTALL_MAN(${bsdtar_MANS})
ENDIF(ENABLE_TAR)

add_subdirectory(test)
