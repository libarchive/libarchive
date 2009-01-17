# - Find lzma
# Find the native LZMA includes and library
#
#  LZMA_INCLUDE_DIR - where to find zlib.h, etc.
#  LZMA_LIBRARIES   - List of libraries when using zlib.
#  LZMA_FOUND       - True if zlib found.

IF (LZMA_INCLUDE_DIR)
  # Already in cache, be silent
  SET(LZMA_FIND_QUIETLY TRUE)
ENDIF (LZMA_INCLUDE_DIR)

FIND_PATH(LZMA_INCLUDE_DIR lzmadec.h)
FIND_LIBRARY(LZMA_LIBRARY NAMES lzmadec )

# handle the QUIETLY and REQUIRED arguments and set LZMA_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LZMA DEFAULT_MSG LZMA_LIBRARY LZMA_INCLUDE_DIR)

IF(LZMA_FOUND)
  SET( LZMA_LIBRARIES ${LZMA_LIBRARY} )
ELSE(LZMA_FOUND)
  SET( LZMA_LIBRARIES )
ENDIF(LZMA_FOUND)

MARK_AS_ADVANCED( LZMA_LIBRARY LZMA_INCLUDE_DIR )
