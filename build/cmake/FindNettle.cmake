# - Find Nettle
# Find the Nettle include directory and library
#
#  NETTLE_INCLUDE_DIR    - where to find <nettle/sha.h>, etc.
#  NETTLE_LIBRARIES      - List of libraries when using libnettle.
#  NETTLE_FOUND          - True if libnettle found.

if (NETTLE_INCLUDE_DIR)
  # Already in cache, be silent
  set(NETTLE_FIND_QUIETLY TRUE)
endif (NETTLE_INCLUDE_DIR)

find_path(NETTLE_INCLUDE_DIR nettle/md5.h nettle/ripemd160.h nettle/sha.h)
find_library(NETTLE_LIBRARY NAMES nettle libnettle)

# handle the QUIETLY and REQUIRED arguments and set NETTLE_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(NETTLE DEFAULT_MSG NETTLE_LIBRARY NETTLE_INCLUDE_DIR)

if(NETTLE_FOUND)
  set(NETTLE_LIBRARIES ${NETTLE_LIBRARY})
endif(NETTLE_FOUND)
