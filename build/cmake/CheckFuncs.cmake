# - Check if the system has the specified type
# CHECK_FUNCS (FUNCTION1 FUNCTION2 ...)
#
#  FUNCTION - the function(s) where the prototype should be declared
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

INCLUDE(CheckFunctionExists)
INCLUDE(CheckSymbolExists)

MACRO (CHECK_FUNCS _FUNCS)
   FOREACH (_func ${ARGV})
      STRING(TOUPPER ${_func} _funcvar)
      SET(_funcvar "HAVE_${_funcvar}")
      SET(_include "")
      IF ("${_funcvar}" MATCHES "^${_funcvar}$")
        IF (WIN32 AND "${_func}" MATCHES "^wmemcmp$")
          SET(_include "wchar.h")
        ENDIF (WIN32 AND "${_func}" MATCHES "^wmemcmp$")
        IF (WIN32 AND "${_func}" MATCHES "^wmemcpy$")
          SET(_include "wchar.h")
        ENDIF (WIN32 AND "${_func}" MATCHES "^wmemcpy$")
        IF ("${_include}" STREQUAL "")
          SET(CHECK_STUB_FUNC_1 "__stub_${_func}")
          SET(CHECK_STUB_FUNC_2 "__stub___${_func}")
          CONFIGURE_FILE( ${CMAKE_SOURCE_DIR}/build/cmake/CheckFuncs_stub.c.in
            ${CMAKE_BINARY_DIR}/cmake.tmp/CheckFuncs_stub.c IMMEDIATE)
          TRY_COMPILE(__stub
            ${CMAKE_BINARY_DIR}
            ${CMAKE_BINARY_DIR}/cmake.tmp/CheckFuncs_stub.c
            COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
            CMAKE_FLAGS
            -DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_INCLUDE_FILE_FLAGS}
            "${CHECK_INCLUDE_FILE_C_INCLUDE_DIRS}")
          IF (__stub)
            SET("${_funcvar}" "" CACHE INTERNAL "Have function ${_func}")
          ELSE (__stub)
            CHECK_FUNCTION_EXISTS("${_func}" "${_funcvar}")
          ENDIF (__stub)
        ELSE ("${_include}" STREQUAL "")
          CHECK_SYMBOL_EXISTS("${_func}" "${_include}" "${_funcvar}")
        ENDIF ("${_include}" STREQUAL "")
      ENDIF ("${_funcvar}" MATCHES "^${_funcvar}$")
   ENDFOREACH (_func)
ENDMACRO (CHECK_FUNCS)

