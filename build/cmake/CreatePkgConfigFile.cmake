# - Generate a libarchive.pc like autotools for pkg-config
#

# Set the required variables (we use the same input file as autotools)
SET(prefix ${CMAKE_INSTALL_PREFIX})
SET(exec_prefix \${prefix})
SET(libdir \${exec_prefix}/lib)
SET(includedir \${prefix}/include)

# Libs.private (@LIBS@): everything that is NOT a pkg-config imported target.
# pkg-config dependencies (PkgConfig::*) are handled through Requires.private
# (@LIBSREQUIRED@) below, so consumers resolve their Libs.private/Cflags.private
# themselves.
SET(_pc_libs "")
FOREACH(mylib ${ADDITIONAL_LIBS})
	IF(TARGET ${mylib})
		# covered by Requires.private
		CONTINUE()
	ELSEIF(mylib MATCHES "^-")
		# already a linker flag
		LIST(APPEND _pc_libs "${mylib}")
	ELSE()
		# Extract the filename from the absolute path
		GET_FILENAME_COMPONENT(mylib_name ${mylib} NAME_WE)
		# Strip the lib prefix
		STRING(REGEX REPLACE "^lib" "" mylib_name ${mylib_name})
		LIST(APPEND _pc_libs "-l${mylib_name}")
	ENDIF()
ENDFOREACH()
# libxml2 found WITHOUT PKG_ENSURE_STATIC: keep these flags.
# If libxml2 goes through PKG_ENSURE_STATIC, delete this loop: it is then
# covered by Requires.private and would only duplicate flags.
FOREACH(mylib ${PC_LIBXML2_STATIC_LDFLAGS})
	LIST(APPEND _pc_libs "${mylib}")
ENDFOREACH()
LIST(REMOVE_DUPLICATES _pc_libs)
STRING(REPLACE ";" " " LIBS "${_pc_libs}")

# Requires.private: module names recorded by PKG_ENSURE_STATIC
# (the macro calls must run before this file is included).
SET(LIBSREQUIRED "")
IF(ARCHIVE_PC_REQUIRES_PRIVATE)
	LIST(REMOVE_DUPLICATES ARCHIVE_PC_REQUIRES_PRIVATE)
	STRING(REPLACE ";" " " LIBSREQUIRED "${ARCHIVE_PC_REQUIRES_PRIVATE}")
ENDIF()

# FIXME: The order of the libraries doesn't take dependencies into account,
#	 thus there's a good chance it'll make some binutils versions unhappy...
#	 This only affects Libs.private (looked up for static builds) though.
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/build/pkgconfig/libarchive.pc.in
		${CMAKE_CURRENT_BINARY_DIR}/build/pkgconfig/libarchive.pc
		@ONLY)
# And install it, of course ;).
IF(ENABLE_INSTALL)
  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/build/pkgconfig/libarchive.pc
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
ENDIF()
