# - Check if the system has the specified type
# CHECK_HEADERS (HEADER1 HEARDER2 ...)
#
#  HEADER - the header(s) where the prototype should be declared
#
# The following variables may be set before calling this macro to
# modify the way the check is run:
#
#  CMAKE_REQUIRED_FLAGS = string of compile command line flags
#  CMAKE_REQUIRED_DEFINITIONS = list of macros to define (-DFOO=bar)
#  CMAKE_REQUIRED_INCLUDES = list of include directories
# Copyright (c) 2009, Michihiro NAKAJIMA
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)

MACRO (CHECK_HEADERS _HEADERS)
   FOREACH (header ${ARGV})
      SET(_files ${header})
      IF("${_files}" MATCHES "^sys/extattr[.]h$")
	SET(_files "sys/types.h" ${header})
      ENDIF("${_files}" MATCHES "^sys/extattr[.]h$")
      IF(CMAKE_HOST_APPLE)
        # Mac OSX requires sys/types.h before sys/acl.h
        IF("${_files}" MATCHES "^sys/acl[.]h$")
  	  SET(_files "sys/types.h" ${header})
        ENDIF("${_files}" MATCHES "^sys/acl[.]h$")
      ENDIF(CMAKE_HOST_APPLE)

      STRING(TOUPPER ${header} headervar)
      SET(headervar "HAVE_${headervar}")
      STRING(REPLACE "/" "_" headervar ${headervar})
      STRING(REPLACE "." "_" headervar ${headervar})
      CHECK_INCLUDE_FILES("${_files}" "${headervar}")
      #MESSAGE(STATUS "${_files} --> ${headervar}")
   ENDFOREACH (header)
ENDMACRO (CHECK_HEADERS)

