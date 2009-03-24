############################################
#
# How to build bsdtar_test
#
############################################
IF(ENABLE_TAR AND ENABLE_TEST)
  SET(bsdtar_test_SOURCES
    ../getdate.c
    main.c
    test.h
    test_0.c
    test_basic.c
    test_copy.c
    test_getdate.c
    test_help.c
    test_option_T.c
    test_option_q.c
    test_option_s.c
    test_patterns.c
    test_stdio.c
    test_strip_components.c
    test_symlink_dir.c
    test_version.c
  )
  IF(WIN32 AND NOT CYGWIN)
    LIST(APPEND bsdtar_test_SOURCES ../../libarchive/archive_windows.c)
    LIST(APPEND bsdtar_test_SOURCES test_windows.c)
  ENDIF(WIN32 AND NOT CYGWIN)

  #
  # Generate the list.h
  #
  GENERATE_LIST_H(${CMAKE_CURRENT_BINARY_DIR}/list.h
    ${bsdtar_test_SOURCES})
  SET_PROPERTY(DIRECTORY APPEND PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_BINARY_DIR})
  #
  # Register target
  #
  ADD_EXECUTABLE(bsdtar_test ${bsdtar_test_SOURCES})
  SET(num 0)
  FOREACH(test ${bsdtar_test_SOURCES})
    IF(test MATCHES "^test_[^/]+[.]c$")
      STRING(REGEX REPLACE "^(test_[^/]+)[.]c$" "\\1" testname ${test})
      ADD_TEST("bsdtar_${testname}" bsdtar_test
        -q -v -p ${BSDTAR} -r ${CMAKE_CURRENT_SOURCE_DIR} ${num})
      MATH(EXPR num "${num} + 1")
    ENDIF(test MATCHES "^test_[^/]+[.]c$")
  ENDFOREACH(test)
ENDIF (ENABLE_TAR AND ENABLE_TEST)
