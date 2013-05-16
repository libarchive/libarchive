# - Find libgcc
# Find the libgcc library.
#
#  LIBGCC_LIBRARIES      - List of libraries when using libgcc
#  LIBGCC_FOUND          - True if libgcc found.

if (LIBGCC_LIBRARY)
  # Already in cache, be silent
  set(LIBGCC_FIND_QUIETLY TRUE)
endif (LIBGCC_LIBRARY)

find_library(LIBGCC_LIBRARY NAMES gcc libgcc)

# handle the QUIETLY and REQUIRED arguments and set LIBGCC_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBGCC DEFAULT_MSG LIBGCC_LIBRARY)

if(LIBGCC_FOUND)
  set(LIBGCC_LIBRARIES ${LIBGCC_LIBRARY})
  set(HAVE_LIBGCC 1)
endif(LIBGCC_FOUND)
