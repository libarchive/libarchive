############################################
#
# How to build bsdcpio_test
#
############################################
IF(ENABLE_CPIO AND ENABLE_TEST)
  SET(bsdcpio_test_SOURCES
    ../cmdline.c
    ../err.c
    ../pathmatch.c
    main.c
    test.h
    test_0.c
    test_basic.c
    test_cmdline.c
    test_format_newc.c
    test_gcpio_compat.c
    test_option_B_upper.c
    test_option_C_upper.c
    test_option_L_upper.c
    test_option_a.c
    test_option_c.c
    test_option_d.c
    test_option_f.c
    test_option_help.c
    test_option_l.c
    test_option_m.c
    test_option_t.c
    test_option_u.c
    test_option_version.c
    test_option_y.c
    test_option_z.c
    test_owner_parse.c
    test_passthrough_dotdot.c
    test_passthrough_reverse.c
    test_pathmatch.c
  )
  IF(WIN32 AND NOT CYGWIN)
    LIST(APPEND bsdcpio_test_SOURCES ../cpio_windows.c)
    LIST(APPEND bsdcpio_test_SOURCES ../cpio_windows.h)
  ENDIF(WIN32 AND NOT CYGWIN)

  #
  # Generate the list.h
  #
  GENERATE_LIST_H(${CMAKE_CURRENT_BINARY_DIR}/list.h
    ${CMAKE_CURRENT_LIST_FILE} ${bsdcpio_test_SOURCES})
  SET_PROPERTY(DIRECTORY APPEND PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_BINARY_DIR})
  #
  # Register target
  #
  ADD_EXECUTABLE(bsdcpio_test ${bsdcpio_test_SOURCES})
  SET_PROPERTY(TARGET bsdcpio_test PROPERTY COMPILE_DEFINITIONS LIST_H)
  SET(num 0)
  FOREACH(test ${bsdcpio_test_SOURCES})
    IF(test MATCHES "^test_[^/]+[.]c$")
      STRING(REGEX REPLACE "^(test_[^/]+)[.]c$" "\\1" testname ${test})
      ADD_TEST("bsdcpio_${testname}" bsdcpio_test
        -q -v -p ${BSDCPIO} -r ${CMAKE_CURRENT_SOURCE_DIR} ${num})
      MATH(EXPR num "${num} + 1")
    ENDIF(test MATCHES "^test_[^/]+[.]c$")
  ENDFOREACH(test)
ENDIF(ENABLE_CPIO AND ENABLE_TEST)

