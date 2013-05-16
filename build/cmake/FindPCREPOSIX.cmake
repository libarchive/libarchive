# - Find pcreposix
# Find the native PCRE and PCREPOSIX include and libraries
#
#  PCRE_INCLUDE_DIR    - where to find pcreposix.h, etc.
#  PCREPOSIX_LIBRARIES - List of libraries when using libpcreposix.
#  PCRE_LIBRARIES      - List of libraries when using libpcre.
#  PCREPOSIX_FOUND     - True if libpcreposix found.
#  PCRE_FOUND          - True if libpcre found.

if (PCRE_INCLUDE_DIR)
  # Already in cache, be silent
  set(PCRE_FIND_QUIETLY TRUE)
endif (PCRE_INCLUDE_DIR)

find_path(PCRE_INCLUDE_DIR pcreposix.h)
find_library(PCREPOSIX_LIBRARY NAMES pcreposix libpcreposix)
find_library(PCRE_LIBRARY NAMES pcre libpcre)

# handle the QUIETLY and REQUIRED arguments and set PCREPOSIX_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PCREPOSIX DEFAULT_MSG PCREPOSIX_LIBRARY PCRE_INCLUDE_DIR)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PCRE DEFAULT_MSG PCRE_LIBRARY)

if(PCREPOSIX_FOUND)
  set(PCREPOSIX_LIBRARIES ${PCREPOSIX_LIBRARY})
  set(HAVE_LIBPCREPOSIX 1)
  set(HAVE_PCREPOSIX_H 1)
endif(PCREPOSIX_FOUND)

if(PCRE_FOUND)
  set(PCRE_LIBRARIES ${PCRE_LIBRARY})
  set(HAVE_LIBPCRE 1)
endif(PCRE_FOUND)
