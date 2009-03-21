#
#
PROJECT(libarchive)
#
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.1)
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build/cmake")
#
# Version - read from 'version' file.
#
FILE(STRINGS ${CMAKE_SOURCE_DIR}/build/version _version)
STRING(REGEX REPLACE
 "^([0-9])[0-9][0-9][0-9][0-9][0-9][0-9][a-z]?$" "\\1" _major ${_version})
STRING(REGEX REPLACE
 "^[0-9]([0-9][0-9][0-9])[0-9][0-9][0-9][a-z]?$" "\\1" _minor ${_version})
STRING(REGEX REPLACE
 "^[0-9][0-9][0-9][0-9]([0-9][0-9][0-9])[a-z]?$" "\\1" _revision ${_version})
STRING(REGEX REPLACE
 "^[0-9][0-9][0-9][0-9][0-9][0-9][0-9]([a-z]?)$" "\\1" _quality ${_version})
SET(_version_number ${_major}${_minor}${_revision})
STRING(REGEX REPLACE "[0]*([^0][0-9]*)$" "\\1" _minor ${_minor})
STRING(REGEX REPLACE "[0]*([^0][0-9]*)$" "\\1" _revision ${_revision})
#
SET(VERSION                    "${_major}.${_minor}.${_revision}${_quality}")
SET(BSDCPIO_VERSION_STRING     "${VERSION}")
SET(BSDTAR_VERSION_STRING      "${VERSION}")
SET(LIBARCHIVE_VERSION_NUMBER  "${_version_number}")
SET(LIBARCHIVE_VERSION_STRING  "${VERSION}")
# Shared library number
SET(SOVERSION                   8)

OPTION(ENABLE_TAR "Enable tar building" ON)
OPTION(ENABLE_TAR_SHARED "Enable dynamic build of tar" OFF)
OPTION(ENABLE_CPIO "Enable cpio building" ON)
OPTION(ENABLE_CPIO_SHARED "Enable dynamic build of cpio" OFF)
OPTION(ENABLE_XATTR "Enable extended attribute support" ON)
OPTION(ENABLE_ACL "Enable ACL support" ON)
OPTION(ENABLE_TEST "Enable unit and regression tests" ON)

IF(ENABLE_TEST)
	ENABLE_TESTING()
ENDIF(ENABLE_TEST)
IF(WIN32 AND NOT CYGWIN)
  # Currently, dynamic build only.
  SET(ENABLE_TAR_SHARED ON)
  SET(ENABLE_CPIO_SHARED ON)
ENDIF(WIN32 AND NOT CYGWIN)

#
INCLUDE(CheckFileOffsetBits)
INCLUDE(CheckFuncs)
INCLUDE(CheckHeaders)
INCLUDE(CheckHeaderDirent)
INCLUDE(CheckHeaderSTDC)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckStructMember)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckTypeExists)
INCLUDE(CheckTypeSize)

#
# Generate list.h
#
MACRO (GENERATE_LIST_H _listfile __list_sources)
  IF (NOT EXISTS "${_listfile}" OR
     ${CMAKE_CURRENT_LIST_FILE} IS_NEWER_THAN "${_listfile}")

    MESSAGE(STATUS "Generating ${_listfile}")
    FILE(WRITE ${_listfile} "")
    FOREACH (testfile ${ARGV})
      IF (testfile MATCHES "^test_[^/]+[.]c$")
        FILE(STRINGS ${testfile} testvar REGEX "^DEFINE_TEST")
        FOREACH (deftest ${testvar})
          FILE(APPEND ${_listfile} "${deftest}\n")
        ENDFOREACH (deftest)
      ENDIF (testfile MATCHES "^test_[^/]+[.]c$")
    ENDFOREACH (testfile)

  ENDIF (NOT EXISTS "${_listfile}" OR
     ${CMAKE_CURRENT_LIST_FILE} IS_NEWER_THAN "${_listfile}")
ENDMACRO (GENERATE_LIST_H)
#
# Generate installation rules for man pages.
#
MACRO (INSTALL_MAN __mans)
  FOREACH (_man ${ARGV})
    STRING(REGEX REPLACE "^.+[.]([1-9])" "\\1" _mansect ${_man})
    INSTALL(FILES ${_man} DESTINATION "share/man/man${_mansect}")
  ENDFOREACH (_man)
ENDMACRO (INSTALL_MAN __mans)

#
# Check compress/decompress libraries
#
IF(WIN32 AND NOT CMAKE_CL_64 AND NOT CYGWIN)
  # GnuWin32 is only for Win32, not Win64.
  SET(__GNUWIN32PATH "C:/Program Files/GnuWin32")
ENDIF(WIN32 AND NOT CMAKE_CL_64 AND NOT CYGWIN)
IF(DEFINED __GNUWIN32PATH AND EXISTS "${__GNUWIN32PATH}")
  # You have to add a path availabel DLL file into PATH environment variable.
  # Maybe DLL path is "C:/Program Files/GnuWin32/bin".
  # The zlib and the bzip2 Setup program have installed programs and DLLs into
  # "C:/Program Files/GnuWin32" by default.
  # This is convenience setting for Windows.
  SET(CMAKE_PREFIX_PATH ${__GNUWIN32PATH} $(CMAKE_PREFIX_PATH))
  #
  # If you didn't use Setup program or installed into nonstandard path,
  # cmake cannot find out your zlib or bzip2 libraries and include files,
  # you should execute cmake with  -DCMAKE_PREFIX_PATH option.
  #   e.g.
  #     cmake -DCMAKE_PREFIX_PATH=<your-GnuWin32-path> <path-to-source>
  #
  # If compiling error occured in zconf.h, You may need patch to zconf.h.
  #--- zconf.h.orig	2005-07-21 00:40:26.000000000
  #+++ zconf.h	2009-01-19 11:39:10.093750000
  #@@ -286,7 +286,7 @@
  # 
  # #if 1           /* HAVE_UNISTD_H -- this line is updated by ./configure */
  # #  include <sys/types.h> /* for off_t */
  #-#  include <unistd.h>    /* for SEEK_* and off_t */
  #+#  include <stdio.h>    /* for SEEK_* and off_t */
  # #  ifdef VMS
  # #    include <unixio.h>   /* for off_t */
  # #  endif
ENDIF(DEFINED __GNUWIN32PATH AND EXISTS "${__GNUWIN32PATH}")

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
  SET(HAVE_LIBLZMA 1)
  SET(HAVE_LZMA_H 1)
  INCLUDE_DIRECTORIES(${LZMA_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${LZMA_LIBRARIES})
ELSEIF(LZMADEC_FOUND)
  SET(HAVE_LIBLZMADEC 1)
  SET(HAVE_LZMADEC_H 1)
  INCLUDE_DIRECTORIES(${LZMADEC_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${LZMADEC_LIBRARIES})
ENDIF(LZMA_FOUND)

#
# Check headers
#
CHECK_HEADER_STDC()
CHECK_HEADER_DIRENT()
# TODO : AC_HEADER_SYS_WAIT
CHECK_INCLUDE_FILE(sys/wait.h       HAVE_SYS_WAIT_H)

CHECK_HEADERS(acl/libacl.h attr/xattr.h ctype.h errno.h)
CHECK_HEADERS(ext2fs/ext2_fs.h fcntl.h grp.h)
CHECK_HEADERS(inttypes.h langinfo.h limits.h linux/fs.h)
CHECK_HEADERS(locale.h paths.h poll.h pwd.h regex.h stdarg.h)
CHECK_HEADERS(stdint.h stdlib.h string.h)
CHECK_HEADERS(sys/acl.h sys/extattr.h sys/ioctl.h sys/mkdev.h)
CHECK_HEADERS(sys/param.h sys/poll.h sys/select.h sys/time.h sys/utime.h)
CHECK_HEADERS(time.h unistd.h utime.h wchar.h wctype.h windows.h)

CHECK_INCLUDE_FILE(direct.h         HAVE_DIRECT_H)
CHECK_INCLUDE_FILE(dlfcn.h          HAVE_DLFCN_H)
CHECK_INCLUDE_FILE(memory.h         HAVE_MEMORY_H)
CHECK_INCLUDE_FILE(process.h        HAVE_PROCESS_H)
CHECK_INCLUDE_FILE(strings.h        HAVE_STRINGS_H)
CHECK_INCLUDE_FILE(sys/stat.h       HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(sys/types.h      HAVE_SYS_TYPES_H)

#
# Find OpenSSL
#
FIND_PACKAGE(OpenSSL)
IF(OPENSSL_FOUND)
  INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${OPENSSL_LIBRARIES})
ENDIF(OPENSSL_FOUND)
#
# Check MD5/RMD160/SHA headers
#
CHECK_HEADERS(openssl/md5.h openssl/ripemd.h openssl/sha.h)
SET(_files "")
IF(HAVE_SYS_TYPES_H)
  SET(_files "sys/types.h")
ENDIF(HAVE_SYS_TYPES_H)
SET(_files ${_files} "md5.h")
CHECK_INCLUDE_FILES("${_files}" HAVE_MD5_H)
CHECK_HEADERS(ripemd.h rmd160.h)
CHECK_HEADERS(sha.h sha1.h sha2.h sha256.h)
#
# Find MD5/RMD160/SHA library
#
FIND_LIBRARY(CRYPTO_LIBRARY NAMES crypto)
IF(CRYPTO_LIBRARY)
  LIST(APPEND ADDITIONAL_LIBS ${CRYPTO_LIBRARY})
ELSE(CRYPTO_LIBRARY)
  IF(NOT OPENSSL_FOUND)
    FIND_LIBRARY(MD_LIBRARY NAMES md)
    IF(MD_LIBRARY)
      LIST(APPEND ADDITIONAL_LIBS ${MD_LIBRARY})
    ENDIF(MD_LIBRARY)
  ENDIF(NOT OPENSSL_FOUND)
ENDIF(CRYPTO_LIBRARY)
#
# Check MD5/RMD160/SHA functions
#
SET(CMAKE_REQUIRED_LIBRARIES ${ADDITIONAL_LIBS})
CHECK_FUNCTION_EXISTS(MD5_Init HAVE_MD5_Init)
IF(NOT HAVE_MD5_Init)
  CHECK_FUNCTION_EXISTS(MD5Init HAVE_MD5Init)
  IF(HAVE_MD5Init)
    SET(MD5_Init, "MD5Init")
    SET(MD5_Update, "MD5Update")
    SET(MD5_Final, "MD5Final")
  ENDIF(HAVE_MD5Init)
ENDIF(NOT HAVE_MD5_Init)
IF(HAVE_MD5_Init OR HAVE_MD5Init)
  SET(HAVE_MD5 1)
ENDIF(HAVE_MD5_Init OR HAVE_MD5Init)
#
CHECK_FUNCTION_EXISTS(RIPEMD160_Init HAVE_RIPEMD160_Init)
IF(NOT HAVE_RIPEMD160_Init)
  CHECK_FUNCTION_EXISTS(RMD160Init HAVE_RMD160Init)
  IF(HAVE_RMD160Init)
    SET(RIPEMD160_Init, "RMD160Init")
    SET(RIPEMD160_Update, "RMD160Update")
    SET(RIPEMD160_Final, "RMD160Final")
  ENDIF(HAVE_RMD160Init)
ENDIF(NOT HAVE_RIPEMD160_Init)
IF(HAVE_RIPEMD160_Init OR HAVE_RMD160Init)
  SET(HAVE_RMD160 1)
ENDIF(HAVE_RIPEMD160_Init OR HAVE_RMD160Init)
#
CHECK_FUNCTION_EXISTS(SHA1_Init HAVE_SHA1_Init)
IF(NOT HAVE_SHA1_Init)
  CHECK_FUNCTION_EXISTS(SHA1Init HAVE_SHA1Init)
  IF(HAVE_SHA1Init)
    SET(SHA1_Init, "SHA1Init")
    SET(SHA1_Update, "SHA1Update")
    SET(SHA1_Final, "SHA1Final")
  ENDIF(HAVE_SHA1Init)
ENDIF(NOT HAVE_SHA1_Init)
IF(HAVE_SHA1_Init OR HAVE_SHA1Init)
  SET(HAVE_SHA1 1)
ENDIF(HAVE_SHA1_Init OR HAVE_SHA1Init)
#
CHECK_FUNCTION_EXISTS(SHA256_Init HAVE_SHA256)
CHECK_FUNCTION_EXISTS(SHA384_Init HAVE_SHA384)
CHECK_FUNCTION_EXISTS(SHA512_Init HAVE_SHA512)

#
# Check functions
#
CHECK_FUNCS(chflags chown chroot)
CHECK_FUNCS(extattr_get_file extattr_list_file)
CHECK_FUNCS(extattr_set_fd extattr_set_file)
CHECK_FUNCS(fchdir fchflags fchmod fchown fcntl fork)
CHECK_FUNCS(fstat ftruncate futimes geteuid getpid)
CHECK_FUNCS(lchflags lchmod lchown)
CHECK_FUNCS(lutimes memmove memset mkdir mkfifo mknod)
CHECK_FUNCS(nl_langinfo pipe poll readlink select setenv setlocale)
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
CHECK_STRUCT_MEMBER("struct stat" st_mtime_n
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIME_N)
CHECK_STRUCT_MEMBER("struct stat" st_umtime
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_UMTIME)
CHECK_STRUCT_MEMBER("struct stat" st_mtime_usec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIME_USEC)
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
# Check for integer types
#
# XXX There must be a way to make this simpler <sigh> XXXX
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
CHECK_TYPE_SIZE(wchar_t     SIZEOF_WCHAR_T)
IF(HAVE_SIZEOF_WCHAR_T)
  SET(HAVE_WCHAR_T 1)
ENDIF(HAVE_SIZEOF_WCHAR_T)
#
# Check if _FILE_OFFSET_BITS macro needed for large files
#
CHECK_FILE_OFFSET_BITS()

#
# Check for Extended Attribute libraries, headers, and functions
#
IF(ENABLE_XATTR)
  CHECK_INCLUDE_FILE(attr/xattr.h     HAVE_ATTR_XATTR_H)
  CHECK_INCLUDE_FILE(sys/xattr.h      HAVE_SYS_XATTR_H)
  CHECK_LIBRARY_EXISTS(attr "setxattr" "" HAVE_ATTR_LIB)
  IF(HAVE_ATTR_LIB)
    SET(CMAKE_REQUIRED_LIBRARIES "attr")
  ENDIF(HAVE_ATTR_LIB)
  CHECK_FUNCS(getxattr lgetxattr listxattr llistxattr fsetxattr lsetxattr)
ENDIF(ENABLE_XATTR)

#
# Check for ACL libraries, headers, and functions
#
# The ACL support in libarchive is written against the POSIX1e draft,
# which was never officially approved and varies quite a bit across
# platforms.  Worse, some systems have completely non-POSIX acl functions,
# which makes the following checks rather more complex than I would like.
#
IF(ENABLE_ACL)
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

ENDIF(ENABLE_ACL)

# Generate "config.h" from "build/cmake/config.h.in"
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/build/cmake/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
ADD_DEFINITIONS(-DHAVE_CONFIG_H)

#
#
#
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/libarchive)
#
IF(MSVC)
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
ENDIF(MSVC)
# Especially for early development, we want to be a little
# aggressive about diagnosing build problems; this can get
# relaxed somewhat in final shipping versions.
IF ("CMAKE_C_COMPILER_ID" MATCHES "^GNU$")
  ADD_DEFINITIONS(-Wall -Werror)
ENDIF ("CMAKE_C_COMPILER_ID" MATCHES "^GNU$")

add_subdirectory(libarchive)
add_subdirectory(tar)
add_subdirectory(cpio)

