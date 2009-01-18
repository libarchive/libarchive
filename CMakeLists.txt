#
#
PROJECT(libarchive)
#
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.1)
SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#
# Version - read from 'version' file.
#
FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/version _version)
STRING(REGEX REPLACE
 "^([0-9])[0-9][0-9][0-9][0-9][0-9][0-9][a-z]?$" "\\1" _major ${_version})
STRING(REGEX REPLACE
 "^[0-9]([0-9][0-9][0-9])[0-9][0-9][0-9][a-z]?$" "\\1" _minor ${_version})
STRING(REGEX REPLACE
 "^[0-9][0-9][0-9][0-9]([0-9][0-9][0-9])[a-z]?$" "\\1" _revision ${_version})
STRING(REGEX REPLACE
 "^[0-9][0-9][0-9][0-9][0-9][0-9][0-9]([a-z]?)$" "\\1" _quality ${_version})
SET(_version_number ${_major}${_minor}${_revision})
STRING(REGEX REPLACE ".*([^0][0-9]*)$" "\\1" _minor ${_minor})
STRING(REGEX REPLACE ".*([^0][0-9]*)$" "\\1" _revision ${_revision})
#
SET(PACKAGE                    "libarchive")
SET(VERSION                    "${_major}.${_minor}.${_revision}${_quality}")
SET(PACKAGE_BUGREPORT          "kientzle@freebsd.org")
SET(BSDCPIO_VERSION_STRING     "1.1.0")
SET(BSDTAR_VERSION_STRING      "${VERSION}")
SET(LIBARCHIVE_VERSION_NUMBER  "${_version_number}")
SET(LIBARCHIVE_VERSION_STRING  "${VERSION}")
SET(PACKAGE_NAME               "${PACKAGE}")
SET(PACKAGE_STRING             "${PACKAGE} ${LIBARCHIVE_VERSION_STRING}")
SET(PACKAGE_TARNAME            "${PACKAGE}")
SET(PACKAGE_VERSION            "${LIBARCHIVE_VERSION_STRING}")
#
# On Windows, currently, bsdtar and bsdcpio are not supported.
#
IF (WIN32)
  SET(NOBUILD_TAR 1)
  SET(NOBUILD_CPIO 1)
ENDIF (WIN32)
#
INCLUDE(CheckLibraryExists)
INCLUDE(CheckFuncs)
INCLUDE(CheckHeaders)
INCLUDE(CheckHeaderDirent)
INCLUDE(CheckHeaderSTDC)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckStructMember)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckTypeExists)
INCLUDE(CheckTypeSize)
#
# For yacc file, generate *.c from *.y
#
MACRO (YACC_FILE _yaccfile)
  FIND_PROGRAM(YACC NAMES yacc bison)
  IF ("YACC" MATCHES "^YACC-NOTFOUND$")
    MESSAGE(FATAL_ERROR "You need a parser geerator such as yacc.")
  ENDIF ("YACC" MATCHES "^YACC-NOTFOUND$")
  GET_FILENAME_COMPONENT(_path ${_yaccfile} PATH)
  GET_FILENAME_COMPONENT(_name ${_yaccfile} NAME_WE)
  SET(_basename "${_path}/${_name}")
  ADD_CUSTOM_COMMAND(
    OUTPUT ${_basename}.c
    COMMAND ${YACC} -o ${_basename}.c ${_yaccfile}
    DEPENDS ${_yaccfile}
    COMMENT "Generating ${_basename}.c from ${_yaccfile}"
  )
ENDMACRO (YACC_FILE)
#
# Generate list.h
#
MACRO (GENERATE_LIST_H __listfile __list_sources)
  IF (NOT "${__listfile}" MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}")
    SET(_listfile "${CMAKE_CURRENT_SOURCE_DIR}/${__listfile}")
  ELSE (NOT "${__listfile}" MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}")
    SET(_listfile "${__listfile}")
  ENDIF (NOT "${__listfile}" MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}")
  IF (NOT EXISTS "${_listfile}" OR
     ${CMAKE_CURRENT_LIST_FILE} IS_NEWER_THAN "${_listfile}")

    MESSAGE(STATUS "Generating ${_listfile}")
    FILE(WRITE ${_listfile} "")
    FOREACH (testfile ${ARGV})
      IF (testfile MATCHES ".+/test_[^/]+[.]c$")
        FILE(STRINGS ${testfile} testvar REGEX "^DEFINE_TEST")
        FOREACH (deftest ${testvar})
          FILE(APPEND ${_listfile} "${deftest}\n")
        ENDFOREACH (deftest)
      ENDIF (testfile MATCHES ".+/test_[^/]+[.]c$")
    ENDFOREACH (testfile)

  ENDIF (NOT EXISTS "${_listfile}" OR
     ${CMAKE_CURRENT_LIST_FILE} IS_NEWER_THAN "${_listfile}")
ENDMACRO (GENERATE_LIST_H)

#
# Check compress/decompress libraries
#
IF(WIN32)
# TODO
  #SET(CMAKE_PREFIX_PATH "C:/Program Files/GnuWin32/lib" $(CMAKE_PREFIX_PATH))
  #SET(ZLIB_INCLUDE_DIR "C:/Program Files/GnuWin32/include")
  #SET(BZIP2_INCLUDE_DIR "C:/Program Files/GnuWin32/include")
ELSE(WIN32)
  # TODO
  SET(CMAKE_PREFIX_PATH "/usr/local/lib" $(CMAKE_PREFIX_PATH))
  SET(ZLIB_INCLUDE_DIR "/usr/local/include")
  SET(BZIP2_INCLUDE_DIR "/usr/local/include")
  SET(LZMA_INCLUDE_DIR "/usr/local/include")
ENDIF(WIN32)

SET(ADDITIONAL_LIBS "")
#
# Find ZLIB
#
FIND_PACKAGE(ZLIB)
IF(ZLIB_FOUND)
  SET(HAVE_LIBZ 1)
  SET(HAVE_ZLIB_H 1)
  INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${ZLIB_LIBRARIES})
ENDIF(ZLIB_FOUND)
#
# Find BZip2
#
FIND_PACKAGE(BZip2)
IF(BZIP2_FOUND)
  SET(HAVE_LIBBZ2 1)
  SET(HAVE_BZLIB_H 1)
  INCLUDE_DIRECTORIES(${BZIP2_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${BZIP2_LIBRARIES})
ENDIF(BZIP2_FOUND)
#
# Find LZMA
#
FIND_PACKAGE(LZMA)
IF(LZMA_FOUND)
  SET(HAVE_LIBLZMADEC 1)
  SET(HAVE_LZMADEC_H 1)
  INCLUDE_DIRECTORIES(${LZMA_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${LZMA_LIBRARIES})
ENDIF(LZMA_FOUND)

#
# Check headers
#
CHECK_HEADER_STDC()
CHECK_HEADER_DIRENT()
# TODO : AC_HEADER_SYS_WAIT
CHECK_INCLUDE_FILE(sys/wait.h       HAVE_SYS_WAIT_H)

CHECK_HEADERS(errno.h ext2fs/ext2_fs.h fcntl.h  grp.h)
CHECK_HEADERS(inttypes.h langinfo.h limits.h linux/fs.h)
CHECK_HEADERS(locale.h paths.h poll.h pwd.h regex.h stdarg.h)
CHECK_HEADERS(stdint.h stdlib.h string.h)
CHECK_HEADERS(sys/acl.h sys/extattr.h sys/ioctl.h)
CHECK_HEADERS(sys/param.h sys/poll.h sys/select.h sys/time.h sys/utime.h)
CHECK_HEADERS(time.h unistd.h utime.h wchar.h wctype.h)

CHECK_INCLUDE_FILE(direct.h         HAVE_DIRECT_H)
CHECK_INCLUDE_FILE(dlfcn.h          HAVE_DLFCN_H)
CHECK_INCLUDE_FILE(memory.h         HAVE_MEMORY_H)
CHECK_INCLUDE_FILE(process.h        HAVE_PROCESS_H)
CHECK_INCLUDE_FILE(strings.h        HAVE_STRINGS_H)
CHECK_INCLUDE_FILE(sys/stat.h       HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(sys/types.h      HAVE_SYS_TYPES_H)

#
# Check functions
#
CHECK_FUNCS(chflags chown chroot extattr_set_fd extattr_set_file)
CHECK_FUNCS(fchdir fchflags fchmod fchown fcntl fork)
CHECK_FUNCS(fstat ftruncate futimes geteuid getpid)
CHECK_FUNCS(lchflags lchmod lchown)
CHECK_FUNCS(lutimes memmove memset mkdir mkfifo mknod)
CHECK_FUNCS(nl_langinfo pipe poll select setenv setlocale)
CHECK_FUNCS(strchr strdup strerror strrchr timegm)
CHECK_FUNCS(tzset unsetenv utime utimes vfork)
CHECK_FUNCS(wcscpy wcslen wctomb wmemcmp wmemcpy)


SET(CMAKE_REQUIRED_LIBRARIES "")
CHECK_SYMBOL_EXISTS(fnmatch          "fnmatch.h"  HAVE_FNMATCH)
CHECK_SYMBOL_EXISTS(fseeko           "stdio.h"    HAVE_FSEEKO)
CHECK_SYMBOL_EXISTS(strerror_r       "string.h"   HAVE_STRERROR_R)
CHECK_SYMBOL_EXISTS(strftime         "time.h"     HAVE_STRFTIME)
CHECK_SYMBOL_EXISTS(vprintf          "stdio.h"    HAVE_VPRINTF)

IF(HAVE_STRERROR_R)
  SET(HAVE_DECL_STRERROR_R 1)
ENDIF(HAVE_STRERROR_R)

#
# Check defines
#
CHECK_SYMBOL_EXISTS(EFTYPE           "errno.h"    HAVE_EFTYPE)
CHECK_SYMBOL_EXISTS(EILSEQ           "errno.h"    HAVE_EILSEQ)
CHECK_SYMBOL_EXISTS(D_MD_ORDER       "langinfo.h" HAVE_D_MD_ORDER)
CHECK_SYMBOL_EXISTS(FNM_LEADING_DIR  "fnmatch.h"  HAVE_FNM_LEADING_DIR)
CHECK_SYMBOL_EXISTS(optarg           "unistd.h"   HAVE_DECL_OPTARG)
CHECK_SYMBOL_EXISTS(optind           "unistd.h"   HAVE_DECL_OPTIND)
IF(HAVE_STDINT_H)
  CHECK_SYMBOL_EXISTS(INT64_MAX      "stdint.h"   HAVE_DECL_INT64_MAX)
  CHECK_SYMBOL_EXISTS(INT64_MIN      "stdint.h"   HAVE_DECL_INT64_MIN)
  CHECK_SYMBOL_EXISTS(UINT32_MAX     "stdint.h"   HAVE_DECL_UINT32_MAX)
  CHECK_SYMBOL_EXISTS(UINT64_MAX     "stdint.h"   HAVE_DECL_UINT64_MAX)
  CHECK_SYMBOL_EXISTS(SIZE_MAX       "stdint.h"   HAVE_DECL_SIZE_MAX)
ELSE(HAVE_STDINT_H)
  CHECK_SYMBOL_EXISTS(INT64_MAX      "limits.h"   HAVE_DECL_INT64_MAX)
  CHECK_SYMBOL_EXISTS(INT64_MIN      "limits.h"   HAVE_DECL_INT64_MIN)
  CHECK_SYMBOL_EXISTS(UINT32_MAX     "limits.h"   HAVE_DECL_UINT32_MAX)
  CHECK_SYMBOL_EXISTS(UINT64_MAX     "limits.h"   HAVE_DECL_UINT64_MAX)
  CHECK_SYMBOL_EXISTS(SIZE_MAX       "limits.h"   HAVE_DECL_SIZE_MAX)
ENDIF(HAVE_STDINT_H)
CHECK_SYMBOL_EXISTS(SSIZE_MAX        "limits.h"   HAVE_DECL_SSIZE_MAX)

#
# Check struct members
#
# Check for birthtime in struct stat
CHECK_STRUCT_MEMBER("struct stat" st_birthtime
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_BIRTHTIME)

# Check for high-resolution timestamps in struct stat
CHECK_STRUCT_MEMBER("struct stat" st_birthtimespec.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC)
CHECK_STRUCT_MEMBER("struct stat" st_mtimespec.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC)
CHECK_STRUCT_MEMBER("struct stat" st_mtim.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
# Check for block size support in struct stat
CHECK_STRUCT_MEMBER("struct stat" st_blksize
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_BLKSIZE)
# Check for st_flags in struct stat (BSD fflags)
CHECK_STRUCT_MEMBER("struct stat" st_flags
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_FLAGS)
#
#
CHECK_STRUCT_MEMBER("struct tm" tm_sec
    "sys/types.h;sys/time.h;time.h" TIME_WITH_SYS_TIME)

#
# Check types
#
CHECK_TYPE_SIZE("long long int"            LONG_LONG_INT)
CHECK_TYPE_SIZE("unsigned long long"       UNSIGNED_LONG_LONG)
CHECK_TYPE_SIZE("unsigned long long int"   UNSIGNED_LONG_LONG_INT)

#
CHECK_TYPE_SIZE(dev_t       DEV_T)
IF(NOT HAVE_DEV_T)
  IF(MSVC)
    SET(dev_t "unsigned int")
  ENDIF(MSVC)
ENDIF(NOT HAVE_DEV_T)
#
CHECK_TYPE_SIZE(gid_t       GID_T)
IF(NOT HAVE_GID_T)
  IF(WIN32)
    SET(gid_t "unsigned int")
  ELSEIF(WIN32)
    SET(gid_t int)
  ENDIF(WIN32)
ENDIF(NOT HAVE_GID_T)
#
CHECK_TYPE_SIZE(id_t        ID_T)
IF(NOT HAVE_ID_T)
  SET(id_t int)
ENDIF(NOT HAVE_ID_T)
#
CHECK_TYPE_SIZE(int64_t     INT64_T)
IF(NOT HAVE_INT64_T)
  IF(MSVC)
    SET(int64_t __int64)
  ENDIF(MSVC)
ENDIF(NOT HAVE_INT64_T)
#
CHECK_TYPE_SIZE(intmax_t    INTMAX_T)
IF(NOT HAVE_INTMAX_T)
  IF(MSVC)
    SET(intmax_t "long long")
  ENDIF(MSVC)
ENDIF(NOT HAVE_INTMAX_T)
#
CHECK_TYPE_SIZE(mode_t      MODE_T)
IF(NOT HAVE_MODE_T)
  IF(MSVC)
    SET(mode_t "unsigned short")
  ELSE(MSVC)
    SET(mode_t "int")
  ENDIF(MSVC)
ENDIF(NOT HAVE_MODE_T)
#
CHECK_TYPE_SIZE(off_t       OFF_T)
IF(NOT HAVE_OFF_T)
  SET(off_t "long long")
ENDIF(NOT HAVE_OFF_T)
#
CHECK_TYPE_SIZE(pid_t       PID_T)
IF(NOT HAVE_PID_T)
  IF(MSVC)
    SET(pid_t "unsigned short")
  ENDIF(MSVC)
ENDIF(NOT HAVE_PID_T)
#
CHECK_TYPE_SIZE(size_t      SIZE_T)
IF(NOT HAVE_SIZE_T)
  SET(size_t "unsigned int")
ENDIF(NOT HAVE_SIZE_T)
#
CHECK_TYPE_SIZE(ssize_t     SSIZE_T)
IF(NOT HAVE_SSIZE_T)
  IF(MSVC)
    IF(CMAKE_CL_64)
      SET(ssize_t "__int64")
    ELSE(CMAKE_CL_64)
      SET(ssize_t   "signed long")
    ENDIF(CMAKE_CL_64)
  ELSE(MSVC)
      SET(ssize_t   "int")
  ENDIF(MSVC)
ENDIF(NOT HAVE_SSIZE_T)
#
CHECK_TYPE_SIZE(uid_t       UID_T)
IF(NOT HAVE_UID_T)
  IF(WIN32)
    SET(uid_t "unsigned int")
  ELSEIF(WIN32)
    SET(uid_t int)
  ENDIF(WIN32)
ENDIF(NOT HAVE_UID_T)
#
CHECK_TYPE_SIZE(uint16_t    UINT16_T)
IF(NOT HAVE_UINT16_T)
  IF(MSVC)
    SET(uint16_t "unsigned short")
  ENDIF(MSVC)
ENDIF(NOT HAVE_UINT16_T)
#
CHECK_TYPE_SIZE(uint32_t    UINT32_T)
IF(NOT HAVE_UINT32_T)
  IF(MSVC)
    SET(uint32_t "unsigned int")
  ENDIF(MSVC)
ENDIF(NOT HAVE_UINT32_T)
#
CHECK_TYPE_SIZE(uint64_t    UINT64_T)
IF(NOT HAVE_UINT64_T)
  IF(MSVC)
    SET(uint64_t "unsigned __int64")
  ENDIF(MSVC)
ENDIF(NOT HAVE_UINT64_T)
#
CHECK_TYPE_SIZE(uintmax_t   UINTMAX_T)
IF(NOT HAVE_UINTMAX_T)
  IF(MSVC)
    SET(uintmax_t "unsigned long long")
  ENDIF(MSVC)
ENDIF(NOT HAVE_UINTMAX_T)

#
# Check for Extended Attributes support
#
IF(NOT DISABLE_XATTR)

  CHECK_INCLUDE_FILE(attr/xattr.h     HAVE_ATTR_XATTR_H)
  CHECK_LIBRARY_EXISTS(attr "setxattr" "" HAVE_ATTR_LIB)
  IF(HAVE_ATTR_LIB)
    SET(CMAKE_REQUIRED_LIBRARIES "attr")
  ENDIF(HAVE_ATTR_LIB)
  CHECK_FUNCS(getxattr lgetxattr listxattr llistxattr fsetxattr lsetxattr)

ENDIF(NOT DISABLE_XATTR)

#
# Check for ACL support
#
# The ACL support in libarchive is written against the POSIX1e draft,
# which was never officially approved and varies quite a bit across
# platforms.  Worse, some systems have completely non-POSIX acl functions,
# which makes the following checks rather more complex than I would like.
#
IF(NOT DISABLE_ACL)

  CHECK_LIBRARY_EXISTS(acl "acl_get_file" "" HAVE_ACL_LIB)
  IF(HAVE_ACL_LIB)
    SET(CMAKE_REQUIRED_LIBRARIES "acl")
  ENDIF(HAVE_ACL_LIB)
  #
  CHECK_FUNCS(acl_create_entry acl_init acl_set_fd acl_set_fd_np acl_set_file)
  CHECK_TYPE_EXISTS(acl_permset_t "sys/types.h;sys/acl.h"    HAVE_ACL_PERMSET_T)

  # The "acl_get_perm()" function was omitted from the POSIX draft.
  # (It's a pretty obvious oversight; otherwise, there's no way to
  # test for specific permissions in a permset.)  Linux uses the obvious
  # name, FreeBSD adds _np to mark it as "non-Posix extension."
  # Test for both as a double-check that we really have POSIX-style ACL support.
  CHECK_SYMBOL_EXISTS(acl_get_perm
      "sys/types.h;sys/acl.h" HAVE_ACL_GET_PERM)
  CHECK_SYMBOL_EXISTS(acl_get_perm_np
      "sys/types.h;sys/acl.h" HAVE_ACL_GET_PERM_NP)

  # MacOS has an acl.h that isn't POSIX.  It can be detected by
  # checking for ACL_USER
  CHECK_SYMBOL_EXISTS(ACL_USER         "sys/acl.h" HAVE_ACL_USER)

ENDIF(NOT DISABLE_ACL)

#
# Generate "config.h" from "config.h.cmake"
#
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
	${CMAKE_CURRENT_SOURCE_DIR}/config.h)

#
# libarchive
#
SET(include_HEADERS
  libarchive/archive.h
  libarchive/archive_write_private.h
)

SET(libarchive_SOURCES
  libarchive/archive_check_magic.c
  libarchive/archive_endian.h
  libarchive/archive_entry.c
  libarchive/archive_entry.h
  libarchive/archive_entry_copy_stat.c
  libarchive/archive_entry_link_resolver.c
  libarchive/archive_entry_private.h
  libarchive/archive_entry_stat.c
  libarchive/archive_entry_strmode.c
  libarchive/archive_platform.h
  libarchive/archive_private.h
  libarchive/archive_read.c
  libarchive/archive_read_data_into_fd.c
  libarchive/archive_read_disk.c
  libarchive/archive_read_disk_entry_from_file.c
  libarchive/archive_read_disk_private.h
  libarchive/archive_read_disk_set_standard_lookup.c
  libarchive/archive_read_extract.c
  libarchive/archive_read_open_fd.c
  libarchive/archive_read_open_file.c
  libarchive/archive_read_open_filename.c
  libarchive/archive_read_open_memory.c
  libarchive/archive_read_private.h
  libarchive/archive_read_support_compression_all.c
  libarchive/archive_read_support_compression_bzip2.c
  libarchive/archive_read_support_compression_compress.c
  libarchive/archive_read_support_compression_gzip.c
  libarchive/archive_read_support_compression_none.c
  libarchive/archive_read_support_compression_program.c
  libarchive/archive_read_support_compression_lzma.c
  libarchive/archive_read_support_format_all.c
  libarchive/archive_read_support_format_ar.c
  libarchive/archive_read_support_format_cpio.c
  libarchive/archive_read_support_format_empty.c
  libarchive/archive_read_support_format_iso9660.c
  libarchive/archive_read_support_format_mtree.c
  libarchive/archive_read_support_format_tar.c
  libarchive/archive_read_support_format_zip.c
  libarchive/archive_string.c
  libarchive/archive_string.h
  libarchive/archive_string_sprintf.c
  libarchive/archive_util.c
  libarchive/archive_virtual.c
  libarchive/archive_write.c
  libarchive/archive_write_disk.c
  libarchive/archive_write_disk_private.h
  libarchive/archive_write_disk_set_standard_lookup.c
  libarchive/archive_write_open_fd.c
  libarchive/archive_write_open_file.c
  libarchive/archive_write_open_filename.c
  libarchive/archive_write_open_memory.c
  libarchive/archive_write_set_compression_bzip2.c
  libarchive/archive_write_set_compression_compress.c
  libarchive/archive_write_set_compression_gzip.c
  libarchive/archive_write_set_compression_none.c
  libarchive/archive_write_set_compression_program.c
  libarchive/archive_write_set_format.c
  libarchive/archive_write_set_format_ar.c
  libarchive/archive_write_set_format_by_name.c
  libarchive/archive_write_set_format_cpio.c
  libarchive/archive_write_set_format_cpio_newc.c
  libarchive/archive_write_set_format_mtree.c
  libarchive/archive_write_set_format_pax.c
  libarchive/archive_write_set_format_shar.c
  libarchive/archive_write_set_format_ustar.c
  libarchive/filter_fork.c
  libarchive/filter_fork.h
)

SET(libarchive_MANS
  libarchive/archive_entry.3
  libarchive/archive_read.3
  libarchive/archive_util.3
  libarchive/archive_write.3
  libarchive/archive_write_disk.3
  libarchive/cpio.5
  libarchive/libarchive.3
  libarchive/libarchive_internals.3
  libarchive/libarchive-formats.5
  libarchive/mtree.5
  libarchive/tar.5
)

IF(WIN32)
  LIST(APPEND libarchive_SOURCES libarchive/archive_windows.c)
  LIST(APPEND libarchive_SOURCES libarchive/archive_windows.h)
ENDIF(WIN32)

#
# For all targes, must read local include files.
#
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/libarchive)
#
ADD_DEFINITIONS(-DHAVE_CONFIG_H)
#
IF(MSVC)
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
ENDIF(MSVC)
#
# Register target
#
ADD_LIBRARY(archive SHARED ${libarchive_SOURCES} ${include_HEADERS})
TARGET_LINK_LIBRARIES(archive ${ADDITIONAL_LIBS})
SET_PROPERTY(TARGET archive PROPERTY RUNTIME_OUTPUT_DIRECTORY
  "${CMAKE_CURRENT_BINARY_DIR}/lib")

SET(libarchive_test_SOURCES
  ${libarchive_SOURCES}
  libarchive/test/main.c
  libarchive/test/read_open_memory.c
  libarchive/test/test.h
  libarchive/test/test_acl_basic.c
  libarchive/test/test_acl_freebsd.c
  libarchive/test/test_acl_pax.c
  libarchive/test/test_archive_api_feature.c
  libarchive/test/test_bad_fd.c
  libarchive/test/test_compat_bzip2.c
  libarchive/test/test_compat_gtar.c
  libarchive/test/test_compat_gzip.c
  libarchive/test/test_compat_tar_hardlink.c
  libarchive/test/test_compat_zip.c
  libarchive/test/test_empty_write.c
  libarchive/test/test_entry.c
  libarchive/test/test_extattr_freebsd.c
  libarchive/test/test_fuzz.c
  libarchive/test/test_link_resolver.c
  libarchive/test/test_pax_filename_encoding.c
  libarchive/test/test_read_compress_program.c
  libarchive/test/test_read_data_large.c
  libarchive/test/test_read_disk.c
  libarchive/test/test_read_extract.c
  libarchive/test/test_read_format_ar.c
  libarchive/test/test_read_format_cpio_bin.c
  libarchive/test/test_read_format_cpio_bin_Z.c
  libarchive/test/test_read_format_cpio_bin_bz2.c
  libarchive/test/test_read_format_cpio_bin_gz.c
  libarchive/test/test_read_format_cpio_odc.c
  libarchive/test/test_read_format_cpio_svr4_gzip.c
  libarchive/test/test_read_format_cpio_svr4c_Z.c
  libarchive/test/test_read_format_empty.c
  libarchive/test/test_read_format_gtar_gz.c
  libarchive/test/test_read_format_gtar_lzma.c
  libarchive/test/test_read_format_gtar_sparse.c
  libarchive/test/test_read_format_iso_gz.c
  libarchive/test/test_read_format_isorr_bz2.c
  libarchive/test/test_read_format_mtree.c
  libarchive/test/test_read_format_pax_bz2.c
  libarchive/test/test_read_format_tar.c
  libarchive/test/test_read_format_tar_empty_filename.c
  libarchive/test/test_read_format_tbz.c
  libarchive/test/test_read_format_tgz.c
  libarchive/test/test_read_format_tz.c
  libarchive/test/test_read_format_zip.c
  libarchive/test/test_read_large.c
  libarchive/test/test_read_pax_truncated.c
  libarchive/test/test_read_position.c
  libarchive/test/test_read_truncated.c
  libarchive/test/test_tar_filenames.c
  libarchive/test/test_tar_large.c
  libarchive/test/test_ustar_filenames.c
  libarchive/test/test_write_compress.c
  libarchive/test/test_write_compress_program.c
  libarchive/test/test_write_disk.c
  libarchive/test/test_write_disk_failures.c
  libarchive/test/test_write_disk_hardlink.c
  libarchive/test/test_write_disk_perms.c
  libarchive/test/test_write_disk_secure.c
  libarchive/test/test_write_disk_times.c
  libarchive/test/test_write_format_ar.c
  libarchive/test/test_write_format_cpio.c
  libarchive/test/test_write_format_cpio_empty.c
  libarchive/test/test_write_format_cpio_odc.c
  libarchive/test/test_write_format_cpio_newc.c
  libarchive/test/test_write_format_pax.c
  libarchive/test/test_write_format_shar_empty.c
  libarchive/test/test_write_format_tar.c
  libarchive/test/test_write_format_tar_empty.c
  libarchive/test/test_write_format_tar_ustar.c
  libarchive/test/test_write_open_memory.c
)

#
# Generate the list.h
#
GENERATE_LIST_H(libarchive/test/list.h ${libarchive_test_SOURCES})
#
# Register target
#
ADD_EXECUTABLE(libarchive_test EXCLUDE_FROM_ALL
    ${libarchive_test_SOURCES} ${include_HEADERS})
TARGET_LINK_LIBRARIES(libarchive_test ${ADDITIONAL_LIBS})
#
# Setting for check-TESTS
#
LIST(APPEND check_program libarchive_test)
SET_PROPERTY(TARGET libarchive_test PROPERTY ENVIRONMENT
  "LIBARCHIVE_TEST_FILES=${CMAKE_CURRENT_SOURCE_DIR}/libarchive/test")


#
# bsdtar
#
IF (NOT NOBUILD_TAR)
  YACC_FILE(tar/getdate.y)
  SET(bsdtar_SOURCES
    tar/bsdtar.c
    tar/bsdtar.h
    tar/bsdtar_platform.h
    tar/cmdline.c
    tar/getdate.c
    tar/matching.c
    tar/read.c
    tar/siginfo.c
    tar/subst.c
    tar/tree.c
    tar/tree.h
    tar/util.c
    tar/write.c
  )

  SET(bsdtar_MANS tar/bsdtar.1)

  #
  # Register target
  #
  ADD_EXECUTABLE(bsdtar ${bsdtar_SOURCES} ${include_HEADERS})
  TARGET_LINK_LIBRARIES(bsdtar archive ${ADDITIONAL_LIBS})
  ADD_DEPENDENCIES(bsdtar archive)

  SET(bsdtar_test_SOURCES
    tar/getdate.c
    tar/test/main.c
    tar/test/test.h
    tar/test/test_0.c
    tar/test/test_basic.c
    tar/test/test_copy.c
    tar/test/test_getdate.c
    tar/test/test_help.c
    tar/test/test_option_T.c
    tar/test/test_option_q.c
    tar/test/test_option_s.c
    tar/test/test_patterns.c
    tar/test/test_stdio.c
    tar/test/test_strip_components.c
    tar/test/test_symlink_dir.c
    tar/test/test_version.c
  )

  #
  # Generate the list.h
  #
  GENERATE_LIST_H(tar/test/list.h ${bsdtar_test_SOURCES})
  #
  # Register target
  #
  ADD_EXECUTABLE(bsdtar_test EXCLUDE_FROM_ALL ${bsdtar_test_SOURCES})
  ADD_DEPENDENCIES(bsdtar_test bsdtar)
  #
  # Setting for check-TESTS
  #
  LIST(APPEND check_program bsdtar_test)
  SET_PROPERTY(TARGET bsdtar_test PROPERTY ENVIRONMENT
    "BSDTAR=${CMAKE_CURRENT_BINARY_DIR}/bsdtar"
    "BSDTAR_TEST_FILES=${CMAKE_CURRENT_SOURCE_DIR}/tar/test")

ENDIF (NOT NOBUILD_TAR)

#
# bsdcpio
#
IF(NOT NOBUILD_CPIO)
  SET(bsdcpio_SOURCES
    cpio/cmdline.c
    cpio/cpio.c
    cpio/cpio.h
    cpio/cpio_platform.h
    cpio/err.c
    cpio/matching.c
    cpio/matching.h
    cpio/pathmatch.c
    cpio/pathmatch.h
  )

  #
  # Register target
  #
  ADD_EXECUTABLE(bsdcpio ${bsdcpio_SOURCES} ${include_HEADERS})
  TARGET_LINK_LIBRARIES(bsdcpio archive ${ADDITIONAL_LIBS})
  ADD_DEPENDENCIES(bsdcpio archive)

  SET(bsdcpio_test_SOURCES
    cpio/cmdline.c
    cpio/err.c
    cpio/pathmatch.c
    cpio/test/main.c
    cpio/test/test.h
    cpio/test/test_0.c
    cpio/test/test_basic.c
    cpio/test/test_format_newc.c
    cpio/test/test_gcpio_compat.c
    cpio/test/test_option_B.c
    cpio/test/test_option_L.c
    cpio/test/test_option_a.c
    cpio/test/test_option_c.c
    cpio/test/test_option_d.c
    cpio/test/test_option_ell.c
    cpio/test/test_option_f.c
    cpio/test/test_option_help.c
    cpio/test/test_option_m.c
    cpio/test/test_option_t.c
    cpio/test/test_option_u.c
    cpio/test/test_option_version.c
    cpio/test/test_option_y.c
    cpio/test/test_option_z.c
    cpio/test/test_owner_parse.c
    cpio/test/test_passthrough_dotdot.c
    cpio/test/test_passthrough_reverse.c
    cpio/test/test_pathmatch.c
  )

  SET(bsdcpio_MANS cpio/bsdcpio.1)

  #
  # Generate the list.h
  #
  GENERATE_LIST_H(cpio/test/list.h ${bsdcpio_test_SOURCES})
  #
  # Register target
  #
  ADD_EXECUTABLE(bsdcpio_test EXCLUDE_FROM_ALL ${bsdcpio_test_SOURCES})
  ADD_DEPENDENCIES(bsdcpio_test bsdcpio)
  #
  # Setting for check-TESTS
  #
  LIST(APPEND check_program bsdcpio_test)
  SET_PROPERTY(TARGET bsdcpio_test PROPERTY ENVIRONMENT
    "BSDCPIO=${CMAKE_CURRENT_BINARY_DIR}/bsdcpio"
    "BSDCPIO_TEST_FILES=${CMAKE_CURRENT_SOURCE_DIR}/cpio/test")

ENDIF(NOT NOBUILD_CPIO)

#
# check-TESTS
#
IF (NOT WIN32)
  #
  # make check-TESTS
  #
  ADD_CUSTOM_TARGET(check-TESTS VERBATIM)
  FOREACH(_test_prog ${check_program})
    ADD_DEPENDENCIES(check-TESTS ${_test_prog})
    GET_PROPERTY(_test_envs TARGET ${_test_prog} PROPERTY ENVIRONMENT)
    FOREACH(_test_env ${_test_envs})
      ADD_CUSTOM_COMMAND(TARGET check-TESTS POST_BUILD
          COMMAND export ${_test_env} ";\\") 
    ENDFOREACH(_test_env)
    ADD_CUSTOM_COMMAND(TARGET check-TESTS POST_BUILD
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${_test_prog})
  ENDFOREACH(_test_prog)
ELSE (NOT WIN32)
  #
  # On Windows, we should make a command script for running test programs.
  #
  ADD_CUSTOM_TARGET(check-TESTS VERBATIM)
  ADD_CUSTOM_COMMAND(TARGET check-TESTS POST_BUILD
        COMMAND "${CMAKE_CURRENT_BINARY_DIR}/check-TESTS.CMD"
                $(ConfigurationName))
  MESSAGE(STATUS "Generating check-TESTS.CMD")
  SET(_testcmd "${CMAKE_CURRENT_BINARY_DIR}/check-TESTS.CMD")
  FILE(WRITE  ${_testcmd} "@rem This file generated by cmake.\n")
  FILE(APPEND ${_testcmd} "@echo off\n")
  FILE(APPEND ${_testcmd} "IF \"%1\" == \"Release\" goto test\n")
  FILE(APPEND ${_testcmd} "IF \"%1\" == \"release\" goto test\n")
  FILE(APPEND ${_testcmd} "IF \"%1\" == \"Debug\" goto test\n")
  FILE(APPEND ${_testcmd} "IF \"%1\" == \"debug\" goto test\n")
  FILE(APPEND ${_testcmd} "goto error\n")
  FILE(APPEND ${_testcmd} ":test\n")
  FOREACH(_test_prog ${check_program})
    GET_PROPERTY(_test_envs TARGET ${_test_prog} PROPERTY ENVIRONMENT)
    FILE(APPEND ${_testcmd} "setlocal\n")
    FOREACH(_test_env ${_test_envs})
      FILE(APPEND ${_testcmd} "SET ${_test_env}\n") 
    ENDFOREACH(_test_env)
    FILE(APPEND ${_testcmd}
        "${CMAKE_CURRENT_BINARY_DIR}/%1/${_test_prog}.exe\n")
    FILE(APPEND ${_testcmd} "endlocal\n")
  ENDFOREACH(_test_prog)
  FILE(APPEND ${_testcmd} "goto end\n")
  FILE(APPEND ${_testcmd} ":error\n")
  FILE(APPEND ${_testcmd} " echo You have to choose Debug or Release\n")
  FILE(APPEND ${_testcmd} ":end\n")
ENDIF (NOT WIN32)

