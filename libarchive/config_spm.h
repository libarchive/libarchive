/*
 * Hand-built configuration for building libarchive via Swift Package Manager.
 * Supports macOS (Apple platforms), Android, Linux, and Windows.
 *
 * Compression library defines (HAVE_ZLIB_H, HAVE_BZLIB_H, etc.) are NOT
 * defined here -- they come from trait-controlled cSettings in Package.swift.
 */

#define __LIBARCHIVE_CONFIG_H_INCLUDED 1

/* ============================================================ */
/* Windows platform configuration                               */
/* ============================================================ */
#if defined(_WIN32) && !defined(__CYGWIN__)

/* Require at least Windows 7 for BCryptDeriveKeyPBKDF2 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06010000
#endif

#include <stdint.h>

/* POSIX types not provided by Windows */
typedef unsigned short uid_t;
typedef unsigned short gid_t;

#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

typedef int pid_t;
typedef unsigned short mode_t;
typedef unsigned int id_t;

#endif /* _WIN32 */

/* ============================================================ */
/* macOS / Apple platform configuration                         */
/* ============================================================ */
#if defined(__APPLE__)

/* ACL support via macOS APIs */
#define ARCHIVE_ACL_DARWIN 1
#define HAVE_ACL_GET_PERM_NP 1
#define HAVE_ACL_GET_LINK_NP 1
#define HAVE_ACL_IS_TRIVIAL_NP 1
#define HAVE_ACL_SET_LINK_NP 1
#define HAVE_SYS_ACL_H 1
#define HAVE_MEMBERSHIP_H 1

/* Extended attributes via macOS APIs */
#define ARCHIVE_XATTR_DARWIN 1
#define HAVE_SYS_XATTR_H 1

/* Crypto via CommonCrypto / libSystem */
#define ARCHIVE_CRYPTO_MD5_LIBSYSTEM 1
#define ARCHIVE_CRYPTO_SHA1_LIBSYSTEM 1
#define ARCHIVE_CRYPTO_SHA256_LIBSYSTEM 1
#define ARCHIVE_CRYPTO_SHA384_LIBSYSTEM 1
#define ARCHIVE_CRYPTO_SHA512_LIBSYSTEM 1

/* macOS-specific features */
#define HAVE_COPYFILE_H 1
#define HAVE_CHFLAGS 1
#define HAVE_FCHFLAGS 1
#define HAVE_LCHFLAGS 1
#define HAVE_STRUCT_STAT_ST_FLAGS 1
#define HAVE_STRUCT_STAT_ST_BIRTHTIME 1
#define HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC 1
#define HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC 1
#define HAVE_ARC4RANDOM_BUF 1
#define HAVE_GETVFSBYNAME 1
#define HAVE_STATFS 1
#define HAVE_FSTATFS 1
#define HAVE_STRUCT_STATFS_F_NAMEMAX 1
#define HAVE_SYS_MOUNT_H 1
#define HAVE_D_MD_ORDER 1
#define HAVE_EFTYPE 1
#define HAVE_READPASSPHRASE 1
#define HAVE_READPASSPHRASE_H 1
#define HAVE_LCHMOD 1

/* macOS has iconv in libc */
#define HAVE_ICONV 1
#define HAVE_ICONV_H 1

/* macOS has libxml2 in SDK */
#define HAVE_LIBXML_XMLREADER_H 1
#define HAVE_LIBXML_XMLWRITER_H 1

/* ============================================================ */
/* Linux platform configuration                                 */
/* ============================================================ */
#elif defined(__linux__)

/* ACL support via POSIX ACL (requires libacl-dev) */
#if __has_include(<sys/acl.h>)
#define ARCHIVE_ACL_LIBACL 1
#define HAVE_ACL_GET_PERM 1
#define HAVE_SYS_ACL_H 1
#endif

/* Extended attributes via Linux APIs (sys/xattr.h is in glibc) */
#if __has_include(<sys/xattr.h>)
#define ARCHIVE_XATTR_LINUX 1
#define HAVE_SYS_XATTR_H 1
#endif
#if __has_include(<attr/xattr.h>)
#define HAVE_ATTR_XATTR_H 1
#endif

/* Crypto via OpenSSL (requires libssl-dev) */
#if __has_include(<openssl/evp.h>)
#define HAVE_LIBCRYPTO 1
#define HAVE_OPENSSL_EVP_H 1
#define HAVE_OPENSSL_MD5_H 1
#define HAVE_OPENSSL_RIPEMD_H 1
#define HAVE_OPENSSL_SHA_H 1
#define HAVE_OPENSSL_SHA256_INIT 1
#define HAVE_OPENSSL_SHA384_INIT 1
#define HAVE_OPENSSL_SHA512_INIT 1
#define HAVE_PKCS5_PBKDF2_HMAC_SHA1 1
#define ARCHIVE_CRYPTO_MD5_OPENSSL 1
#define ARCHIVE_CRYPTO_RMD160_OPENSSL 1
#define ARCHIVE_CRYPTO_SHA1_OPENSSL 1
#define ARCHIVE_CRYPTO_SHA256_OPENSSL 1
#define ARCHIVE_CRYPTO_SHA384_OPENSSL 1
#define ARCHIVE_CRYPTO_SHA512_OPENSSL 1
#endif

/* Linux-specific features */
#define HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC 1
#define HAVE_FUTIMENS 1
#define HAVE_UTIMENSAT 1
#define HAVE_LINUX_FIEMAP_H 1
#define HAVE_LINUX_FS_H 1
#define HAVE_LINUX_TYPES_H 1
#define HAVE_LINUX_MAGIC_H 1
#define HAVE_SYS_STATFS_H 1
#define HAVE_STATFS 1
#define HAVE_FSTATFS 1

/* iconv */
#define HAVE_ICONV 1
#define HAVE_ICONV_H 1

/* libxml2 - may or may not be available */
/* #define HAVE_LIBXML_XMLREADER_H 1 */

#endif /* platform */

/* ============================================================ */
/* Common POSIX defines (shared across macOS and Linux)         */
/* ============================================================ */

#define HAVE_CHOWN 1
#define HAVE_CHROOT 1
#define HAVE_CTIME_R 1
#define HAVE_CTYPE_H 1
#define HAVE_DECL_INT32_MAX 1
#define HAVE_DECL_INT32_MIN 1
#define HAVE_DECL_INT64_MAX 1
#define HAVE_DECL_INT64_MIN 1
#define HAVE_DECL_INTMAX_MAX 1
#define HAVE_DECL_INTMAX_MIN 1
#define HAVE_DECL_SIZE_MAX 1
#define HAVE_DECL_SSIZE_MAX 1
#define HAVE_DECL_STRERROR_R 1
#define HAVE_DECL_UINT32_MAX 1
#define HAVE_DECL_UINT64_MAX 1
#define HAVE_DECL_UINTMAX_MAX 1
#define HAVE_DIRENT_H 1
#define HAVE_DLFCN_H 1
#define HAVE_EILSEQ 1
#define HAVE_ERRNO_H 1
#define HAVE_FCHDIR 1
#define HAVE_FCHMOD 1
#define HAVE_FCHOWN 1
#define HAVE_FCNTL 1
#define HAVE_FCNTL_H 1
#define HAVE_FDOPENDIR 1
#define HAVE_FNMATCH 1
#define HAVE_FNMATCH_H 1
#define HAVE_FORK 1
#define HAVE_FSEEKO 1
#define HAVE_FSTAT 1
#define HAVE_FSTATAT 1
#define HAVE_FSTATVFS 1
#define HAVE_FTRUNCATE 1
#define HAVE_FUTIMES 1
#define HAVE_GETEUID 1
#define HAVE_GETGRGID_R 1
#define HAVE_GETGRNAM_R 1
#define HAVE_GETLINE 1
#define HAVE_GETPID 1
#define HAVE_GETPWNAM_R 1
#define HAVE_GETPWUID_R 1
#define HAVE_GMTIME_R 1
#define HAVE_GRP_H 1
#define HAVE_INTMAX_T 1
#define HAVE_INTTYPES_H 1
#define HAVE_LANGINFO_H 1
#define HAVE_LCHOWN 1
#define HAVE_LIMITS_H 1
#define HAVE_LINK 1
#define HAVE_LINKAT 1
#define HAVE_LOCALE_H 1
#define HAVE_LOCALTIME_R 1
#define HAVE_LONG_LONG_INT 1
#define HAVE_LSTAT 1
#define HAVE_LUTIMES 1
#define HAVE_MBRTOWC 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMORY_H 1
#define HAVE_MEMSET 1
#define HAVE_MKDIR 1
#define HAVE_MKFIFO 1
#define HAVE_MKNOD 1
#define HAVE_MKSTEMP 1
#define HAVE_NL_LANGINFO 1
#define HAVE_OPENAT 1
#define HAVE_PATHS_H 1
#define HAVE_PIPE 1
#define HAVE_POLL 1
#define HAVE_POLL_H 1
#define HAVE_POSIX_SPAWNP 1
#define HAVE_PTHREAD_H 1
#define HAVE_PWD_H 1
#define HAVE_READDIR_R 1
#define HAVE_READLINK 1
#define HAVE_READLINKAT 1
#define HAVE_REGEX_H 1
#define HAVE_SELECT 1
#define HAVE_SETENV 1
#define HAVE_SETLOCALE 1
#define HAVE_SIGACTION 1
#define HAVE_SIGNAL_H 1
#define HAVE_SPAWN_H 1
#define HAVE_STATVFS 1
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRERROR_R 1
#define HAVE_STRFTIME 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRNLEN 1
#define HAVE_STRRCHR 1
#define HAVE_STRUCT_STAT_ST_BLKSIZE 1
#define HAVE_STRUCT_TM_TM_GMTOFF 1
#define HAVE_SYMLINK 1
#define HAVE_SYS_CDEFS_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_POLL_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_STATVFS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UTSNAME_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_TIMEGM 1
#define HAVE_TIME_H 1
#define HAVE_TZSET 1
#define HAVE_UINTMAX_T 1
#define HAVE_UNISTD_H 1
#define HAVE_UNLINKAT 1
#define HAVE_UNSETENV 1
#define HAVE_UNSIGNED_LONG_LONG 1
#define HAVE_UNSIGNED_LONG_LONG_INT 1
#define HAVE_UTIME 1
#define HAVE_UTIMES 1
#define HAVE_UTIME_H 1
#define HAVE_VFORK 1
#define HAVE_VPRINTF 1
#define HAVE_WCHAR_H 1
#define HAVE_WCHAR_T 1
#define HAVE_WCRTOMB 1
#define HAVE_WCSCMP 1
#define HAVE_WCSCPY 1
#define HAVE_WCSLEN 1
#define HAVE_WCTOMB 1
#define HAVE_WCTYPE_H 1
#define HAVE_WMEMCMP 1
#define HAVE_WMEMCPY 1
#define HAVE_WMEMMOVE 1

/* iconv const qualifier (empty on macOS and glibc) */
#define ICONV_CONST

/* Hash support flags */
#define HAVE_SHA256 1
#define HAVE_SHA384 1
#define HAVE_SHA512 1

/* ============================================================ */
/* Windows (MSVC / clang-cl) overrides                           */
/* Windows lacks most POSIX headers and functions; undef them.   */
/* ============================================================ */
#if defined(_WIN32) && !defined(__CYGWIN__)
#undef HAVE_CHOWN
#undef HAVE_CHROOT
#undef HAVE_CTIME_R
#undef HAVE_DECL_SSIZE_MAX
#undef HAVE_DECL_STRERROR_R
#undef HAVE_DIRENT_H
#undef HAVE_DLFCN_H
#undef HAVE_FCHDIR
#undef HAVE_FCHMOD
#undef HAVE_FCHOWN
#undef HAVE_FCNTL
#undef HAVE_FDOPENDIR
#undef HAVE_FNMATCH
#undef HAVE_FNMATCH_H
#undef HAVE_FORK
#undef HAVE_FSEEKO
#undef HAVE_FSTATAT
#undef HAVE_FSTATVFS
#undef HAVE_FTRUNCATE
#undef HAVE_FUTIMES
#undef HAVE_GETEUID
#undef HAVE_GETGRGID_R
#undef HAVE_GETGRNAM_R
#undef HAVE_GETLINE
#undef HAVE_GETPWNAM_R
#undef HAVE_GETPWUID_R
#undef HAVE_GMTIME_R
#undef HAVE_GRP_H
#undef HAVE_LANGINFO_H
#undef HAVE_LCHOWN
#undef HAVE_LINK
#undef HAVE_LINKAT
#undef HAVE_LOCALTIME_R
#undef HAVE_LSTAT
#undef HAVE_LUTIMES
#undef HAVE_MKFIFO
#undef HAVE_MKNOD
#undef HAVE_MKSTEMP
#undef HAVE_NL_LANGINFO
#undef HAVE_OPENAT
#undef HAVE_PATHS_H
#undef HAVE_PIPE
#undef HAVE_POLL
#undef HAVE_POLL_H
#undef HAVE_POSIX_SPAWNP
#undef HAVE_PTHREAD_H
#undef HAVE_PWD_H
#undef HAVE_READDIR_R
#undef HAVE_READLINK
#undef HAVE_READLINKAT
#undef HAVE_REGEX_H
#undef HAVE_SIGACTION
#undef HAVE_SPAWN_H
#undef HAVE_STATVFS
#undef HAVE_STRERROR_R
#undef HAVE_STRINGS_H
#undef HAVE_STRUCT_STAT_ST_BLKSIZE
#undef HAVE_STRUCT_TM_TM_GMTOFF
#undef HAVE_SYMLINK
#undef HAVE_SYS_CDEFS_H
#undef HAVE_SYS_IOCTL_H
#undef HAVE_SYS_PARAM_H
#undef HAVE_SYS_POLL_H
#undef HAVE_SYS_SELECT_H
#undef HAVE_SYS_STATVFS_H
#undef HAVE_SYS_TIME_H
#undef HAVE_SYS_UTSNAME_H
#undef HAVE_SYS_WAIT_H
#undef HAVE_TIMEGM
#undef HAVE_UNISTD_H
#undef HAVE_UNLINKAT
#undef HAVE_UNSETENV
#undef HAVE_UTIMES
#undef HAVE_UTIME_H
#undef HAVE_VFORK
#undef HAVE_ICONV
#undef HAVE_ICONV_H
#undef ICONV_CONST
/* Windows UCRT provides _get_timezone() instead of the POSIX timezone global */
#define HAVE__GET_TIMEZONE 1
/* Windows BCrypt API for random number generation */
#define HAVE_BCRYPT_H 1
/* Crypto via Windows CNG (BCrypt) */
#define ARCHIVE_CRYPTO_MD5_WIN 1
#define ARCHIVE_CRYPTO_SHA1_WIN 1
#define ARCHIVE_CRYPTO_SHA256_WIN 1
#define ARCHIVE_CRYPTO_SHA384_WIN 1
#define ARCHIVE_CRYPTO_SHA512_WIN 1
#endif /* _WIN32 */

/* ============================================================ */
/* Android (Bionic libc) overrides                               */
/* Bionic lacks several POSIX/BSD functions that glibc provides. */
/* ============================================================ */
#if defined(__ANDROID__)
#undef HAVE_CHROOT
#undef HAVE_FUTIMES
#undef HAVE_LANGINFO_H
#undef HAVE_LUTIMES
#undef HAVE_MKFIFO
#undef HAVE_MKNOD
#undef HAVE_NL_LANGINFO
#undef HAVE_PATHS_H
#undef HAVE_READDIR_R
#undef HAVE_READPASSPHRASE
#undef HAVE_READPASSPHRASE_H
#undef HAVE_SYS_CDEFS_H
#undef HAVE_VFORK
/* Android iconv may not be available */
#if !__has_include(<iconv.h>)
#undef HAVE_ICONV
#undef HAVE_ICONV_H
#endif
#endif /* __ANDROID__ */

/* ============================================================ */
/* Validate compression headers actually exist.                  */
/* SPM traits are package-level, so defines like HAVE_BZLIB_H    */
/* may be set even when cross-compiling to platforms that lack    */
/* the headers. Undef them if the header is not actually present. */
/* ============================================================ */
#if defined(HAVE_ZLIB_H) && !__has_include(<zlib.h>)
#undef HAVE_ZLIB_H
#undef HAVE_LIBZ
#endif

#if defined(HAVE_BZLIB_H) && !__has_include(<bzlib.h>)
#undef HAVE_BZLIB_H
#undef HAVE_LIBBZ2
#endif

#if defined(HAVE_LZMA_H) && !__has_include(<lzma.h>)
#undef HAVE_LZMA_H
#undef HAVE_LIBLZMA
#undef HAVE_LZMA_STREAM_ENCODER_MT
#endif

#if defined(HAVE_ZSTD_H) && !__has_include(<zstd.h>)
#undef HAVE_ZSTD_H
#undef HAVE_LIBZSTD
#undef HAVE_ZSTD_compressStream
#endif
