/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */
#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED


///////////////////////////////////////////////////////////////////////////
//  Check for Watcom and Microsoft Visual C compilers (WIN32 only)  ///////
///////////////////////////////////////////////////////////////////////////
#if defined(__WIN32__) || defined(_WIN32) || defined(__WIN32)
  #define IS_WIN32  1

  #if defined(__TURBOC__) || defined(__BORLANDC__) /* Borland compilers */
  #elif defined( __WATCOMC__ ) || defined(__WATCOMCPP__) /* Watcom compilers */
    #define IS_WATCOM  1
    /* Define to 1 if __INT64 is defined */
    #define HAVE___INT64  1

    /* Define to 1 if UID should be unsigned */
    #define USE_UNSIGNED_UID 1

    /* Define to 1 if UID should be unsigned */
    #define USE_UNSIGNED_GID 1

  #elif defined(__IBMC__) || defined(__IBMCPP__) /* IBM compilers */
  #elif defined( __SC__ ) /* Symantec C++ compilers */
  #elif defined( M_I86 ) && defined( MSDOS ) /* Microsoft DOS/Win 16 compilers */
  #elif defined( _M_IX86 ) || defined( _68K_ ) /* Microsoft Win32 compilers */
    #define IS_VISUALC 1
    /* Define to 1 if __INT64 is defined */
    #define HAVE___INT64  1
  #else
  #endif

#endif
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/* Define to 1 if you have the `acl_create_entry' function. */
/* #undef HAVE_ACL_CREATE_ENTRY */

/* Define to 1 if you have the `acl_get_perm' function. */
/* #undef HAVE_ACL_GET_PERM */

/* Define to 1 if you have the `acl_get_perm_np' function. */
/* #undef HAVE_ACL_GET_PERM_NP */

/* Define to 1 if you have the `acl_init' function. */
/* #undef HAVE_ACL_INIT */

/* Define to 1 if the system has the type `acl_permset_t'. */
/* #undef HAVE_ACL_PERMSET_T */

/* Define to 1 if you have the `acl_set_fd' function. */
/* #undef HAVE_ACL_SET_FD */

/* Define to 1 if you have the `acl_set_fd_np' function. */
/* #undef HAVE_ACL_SET_FD_NP */

/* Define to 1 if you have the `acl_set_file' function. */
/* #undef HAVE_ACL_SET_FILE */

/* True for systems with POSIX ACL support */
/* #undef HAVE_ACL_USER */

/* Define to 1 if you have the <attr/xattr.h> header file. */
/* #undef HAVE_ATTR_XATTR_H */

/* Define to 1 if you have the <bzlib.h> header file. */
/* #undef HAVE_BZLIB_H */

/* Define to 1 if you have the `chflags' function. */
/* #undef HAVE_CHFLAGS */

/* Define to 1 if you have the `chown' function. */
/* #undef HAVE_CHOWN */

/* Define to 1 if you have the declaration of `INT64_MAX', and to 0 if you
   don't. */
/* #undef HAVE_DECL_INT64_MAX */

/* Define to 1 if you have the declaration of `INT64_MIN', and to 0 if you
   don't. */
/* #undef HAVE_DECL_INT64_MIN */

/* Define to 1 if you have the declaration of `optarg', and to 0 if you don't.
   */
/* #undef HAVE_DECL_OPTARG */

/* Define to 1 if you have the declaration of `optind', and to 0 if you don't.
   */
/* #undef HAVE_DECL_OPTIND */

/* Define to 1 if you have the declaration of `SIZE_MAX', and to 0 if you
   don't. */
/* #undef HAVE_DECL_SIZE_MAX */

#if defined(_MSC_VER) && _MSC_VER >= 1400
#define HAVE_DECL_SIZE_MAX 1
#endif

/* Define to 1 if you have the declaration of `strerror_r', and to 0 if you
   don't. */
/* #undef HAVE_DECL_STRERROR_R */

/* Define to 1 if you have the declaration of `UINT32_MAX', and to 0 if you
   don't. */
/* #undef HAVE_DECL_UINT32_MAX */

/* Define to 1 if you have the declaration of `UINT64_MAX', and to 0 if you
   don't. */
/* #undef HAVE_DECL_UINT64_MAX */

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_DIRENT_H */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if nl_langinfo supports D_MD_ORDER */
/* #undef HAVE_D_MD_ORDER */

/* A possible errno value for invalid file format errors */
#if ((IS_WATCOM) || (IS_VISUALC))
#define HAVE_EFTYPE 0
#else
#define HAVE_EFTYPE 1
#endif

/* A possible errno value for invalid file format errors */
#define HAVE_EILSEQ 1

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <ext2fs/ext2_fs.h> header file. */
/* #undef HAVE_EXT2FS_EXT2_FS_H */

/* Define to 1 if you have the `fchdir' function. */
/* #undef HAVE_FCHDIR */

/* Define to 1 if you have the `fchflags' function. */
/* #undef HAVE_FCHFLAGS */

/* Define to 1 if you have the `fchmod' function. */
/* #undef HAVE_FCHMOD */

/* Define to 1 if you have the `fchown' function. */
/* #undef HAVE_FCHOWN */

/* Define to 1 if you have the <fcntl.h> header file. */
/* #undef HAVE_FCNTL_H 1 */

/* Define to 1 if you have the fcntl() function. */
/* #undef HAVE_FCNTL_FN */

/* Define to 1 if your system has a working POSIX `fnmatch' function. */
/* #undef HAVE_FNMATCH */

/* Define to 1 if fnmatch(3) supports the FNM_LEADING_DIR flag */
/* #undef HAVE_FNM_LEADING_DIR */

/* Define to 1 if fseeko (and presumably ftello) exists and is declared. */
/* #undef HAVE_FSEEKO */

/* Define to 1 if you have the `fsetxattr' function. */
/* #undef HAVE_FSETXATTR */

/* Define to 1 if you have the `ftruncate' function. */
/* #undef HAVE_FTRUNCATE */

/* Define to 1 if you have the `futimes' function. */
/* #undef HAVE_FUTIMES */

/* Define to 1 if you have the `geteuid' function. */
/* #undef HAVE_GETEUID */

/* Define to 1 if you have the `getopt_long' function. */
/* #undef HAVE_GETOPT_LONG */

/* Define to 1 if you have the `getxattr' function. */
/* #undef HAVE_GETXATTR */

/* Define to 1 if you have the <grp.h> header file. */
/* #undef HAVE_GRP_H */

/* Define to 1 if the system has the type `intmax_t'. */
/* #undef HAVE_INTMAX_T */

/* Define to 1 if you have the <inttypes.h> header file. */
/* #undef HAVE_INTTYPES_H */

/* Define to 1 if you have the <langinfo.h> header file. */
/* #undef HAVE_LANGINFO_H */

/* Define to 1 if you have the `lchflags' function. */
/* #undef HAVE_LCHFLAGS */

/* Define to 1 if you have the `lchmod' function. */
/* #undef HAVE_LCHMOD */

/* Define to 1 if you have the `lchown' function. */
/* #undef HAVE_LCHOWN */

/* Define to 1 if you have the `lgetxattr' function. */
/* #undef HAVE_LGETXATTR */

/* Define to 1 if you have the `acl' library (-lacl). */
/* #undef HAVE_LIBACL */

/* Define to 1 if you have the `attr' library (-lattr). */
/* #undef HAVE_LIBATTR */

/* Define to 1 if you have the `bz2' library (-lbz2). */
/* #undef HAVE_LIBBZ2 */

/* Define to 1 if you have the `z' library (-lz). */
/* #undef HAVE_LIBZ */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <linux/ext2_fs.h> header file. */
/* #undef HAVE_LINUX_EXT2_FS_H */

/* Define to 1 if you have the <linux/fs.h> header file. */
/* #undef HAVE_LINUX_FS_H */

/* Define to 1 if you have the `listxattr' function. */
/* #undef HAVE_LISTXATTR */

/* Define to 1 if you have the `llistxattr' function. */
/* #undef HAVE_LLISTXATTR */

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if the system has the type `long long int'. */
#define HAVE_LONG_LONG_INT 1

/* Define to 1 if you have the `lsetxattr' function. */
/* #undef HAVE_LSETXATTR */

/* Define to 1 if `lstat' has the bug that it succeeds when given the
   zero-length file name argument. */
/* #undef HAVE_LSTAT_EMPTY_STRING_BUG */

/* Define to 1 if you have the `lutimes' function. */
/* #undef HAVE_LUTIMES */

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the `mkdir' function. */
#define HAVE_MKDIR 1

/* Define to 1 if you have the `mkfifo' function. */
/* #undef HAVE_MKFIFO */

/* Define to 1 if you have the `mknod' function. */
/* #undef HAVE_MKNOD */

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the `nl_langinfo' function. */
/* #undef HAVE_NL_LANGINFO */

/* Define to 1 if you have the <paths.h> header file. */
/* #undef HAVE_PATHS_H */

/* Define to 1 if you have the `poll' function. */
/* #undef HAVE_POLL */

/* Define to 1 if you have the <poll.h> header file. */
/* #undef HAVE_POLL_H */

/* Define to 1 if you have the <pwd.sh.h> header file. */
/* #undef HAVE_PWD_H */

/* Define to 1 if you have the `select' function. */
/* #undef HAVE_SELECT */

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if `stat' has the bug that it succeeds when given the
   zero-length file name argument. */
/* #undef HAVE_STAT_EMPTY_STRING_BUG */

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stdint.h> header file. */
/* #undef HAVE_STDINT_H */

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the `strerror_r' function. */
/* #undef HAVE_STRERROR_R */

/* Define to 1 if you have the `strftime' function. */
#define HAVE_STRFTIME 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if `st_mtimespec.tv_nsec' is member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC */

/* Define to 1 if `st_mtim.tv_nsec' is member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC */

/* Define to 1 if you have the <sys/acl.h> header file. */
/* #undef HAVE_SYS_ACL_H */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
/* #undef HAVE_SYS_IOCTL_H */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #undef HAVE_SYS_PARAM_H */

/* Define to 1 if you have the <sys/poll.h> header file. */
/* #undef HAVE_SYS_POLL_H */

/* Define to 1 if you have the <sys/select.h> header file. */
/* #undef HAVE_SYS_SELECT_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
/* #undef HAVE_SYS_TIME_H */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/utime.h> header file. */
#define HAVE_SYS_UTIME_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
/* #undef HAVE_SYS_WAIT_H */

/* Define to 1 if you have the `timegm' function. */
/* #undef HAVE_TIMEGM */

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if the system has the type `uintmax_t'. */
/* #undef HAVE_UINTMAX_T */

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */

/* Define to 1 if the system has the type `unsigned long long'. */
#define HAVE_UNSIGNED_LONG_LONG 1

/* Define to 1 if the system has the type `unsigned long long int'. */
#define HAVE_UNSIGNED_LONG_LONG_INT 1

/* Define to 1 if you have the `utime' function. */
#define HAVE_UTIME 1

/* Define to 1 if you have the `utimes' function. */
/* #undef HAVE_UTIMES */

/* Define to 1 if you have the <utime.h> header file. */
/* #undef HAVE_UTIME_H */

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the `wcscpy' function. */
#define HAVE_WCSCPY 1

/* Define to 1 if you have the `wcslen' function. */
#define HAVE_WCSLEN 1

/* Define to 1 if you have the `wmemcmp' function. */
/* #undef HAVE_WMEMCMP */

/* Define to 1 if you have the `wmemcpy' function. */
/* #undef HAVE_WMEMCPY */

/* Define to 1 if you have the <zlib.h> header file. */
#define HAVE_ZLIB_H 1

/* Version number of libarchive as a single integer */
#define LIBARCHIVE_VERSION_NUMBER "2005000"

/* Version number of libarchive */
#define LIBARCHIVE_VERSION_STRING "2.5.0b"

/* Define to 1 if `lstat' dereferences a symlink specified with a trailing
   slash. */
/* #undef LSTAT_FOLLOWS_SLASHED_SYMLINK */

/* Define to 1 if `major', `minor', and `makedev' are declared in <mkdev.h>.
   */
/* #undef MAJOR_IN_MKDEV */

/* Define to 1 if `major', `minor', and `makedev' are declared in
   <sysmacros.h>. */
/* #undef MAJOR_IN_SYSMACROS */

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Name of package */
#define PACKAGE "libarchive"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "kientzle@freebsd.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "libarchive"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libarchive 2.4.12"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libarchive"

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.4.12"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if strerror_r returns char *. */
/* #undef STRERROR_R_CHAR_P */

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Version number of package */
#define VERSION "2.4.12"

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define to 1 to make fseeko visible on some hosts (e.g. glibc 2.2). */
/* #undef _LARGEFILE_SOURCE */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define for Solaris 2.5.1 so the uint64_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef was allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT64_T */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define. */
#if (USE_UNSIGNED_GID)
#define gid_t unsigned int
#else
#define gid_t int
#endif

/* Define to `unsigned long' if <sys/types.h> does not define. */
#define id_t int

/* Define to the type of a signed integer type of width exactly 64 bits if
   such a type exists and the standard includes do not define it. */
#if (HAVE___INT64)
typedef __int64 int64_t;
#else
#define int64_t long long
#endif

/* Define to the widest signed integer type if <stdint.h> and <inttypes.h> do
   not define. */
#if (HAVE___INT64)
typedef __int64 intmax_t;
#else
#define intmax_t long long
#endif

/* Define to `int' if <sys/types.h> does not define. */
#define mode_t unsigned short

/* Define to `long long' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> doesn't define. */
#if (USE_UNSIGNED_UID)
#define uid_t unsigned int
#else
#define uid_t int
#endif

/* Define to the type of an unsigned integer type of width exactly 64 bits if
   such a type exists and the standard includes do not define it. */
#if (HAVE___INT64)
typedef unsigned __int64 uint64_t;
#else
#define uint64_t unsigned long long
#endif

/* Define to the widest unsigned integer type if <stdint.h> and <inttypes.h>
   do not define. */
#if (HAVE___INT64)
typedef unsigned __int64 uintmax_t;
#else
#define uintmax_t unsigned long long
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef uintptr_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
#define pid_t unsigned int

#define uint32_t unsigned long
#define uint16_t unsigned short
#define ssize_t long


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#if (IS_VISUALC)
  #include <io.h>
  #include <stdlib.h>   //brings in NULL
  #include <sys/stat.h> //brings in S_IFMT(), etc...

  #define HAVE_UINTPTR  0

  #if !defined(STDIN_FILENO)
    #define STDIN_FILENO 0
  #endif

  #if !defined(STDOUT_FILENO)
    #define STDOUT_FILENO 1
  #endif

  #if !defined(STDERR_FILENO)
    #define STDERR_FILENO 2
  #endif

  /* Define to 1 if ino_t is defined (possibly in sys/types.h) */
  #define HAVE_INO_T  1

  #define S_IFFIFO         _S_IFIFO

  #define S_ISBLK( m )     0
  #define S_ISFIFO( m )    (((m) & S_IFMT) == S_IFFIFO)
  #define S_ISCHR( m )     (((m) & S_IFMT) == S_IFCHR)
  #define S_ISDIR( m )     (((m) & S_IFMT) == S_IFDIR)
  #define S_ISREG( m )     (((m) & S_IFMT) == S_IFREG)
  #define S_ISUID  0004000
  #define S_ISGID  0002000
  #define S_ISVTX  0001000

  //NOT SURE IF O_NONBLOCK is OK here but at least the 0x0004 flag is not used by anything else...
  #define O_NONBLOCK 0x0004 /* Non-blocking I/O.  */
  //#define O_NDELAY   O_NONBLOCK

  #define lstat  _stat

  /* Symbolic constants for the access() function */
  #if !defined(F_OK)
    #define R_OK    4       /*  Test for read permission    */
    #define W_OK    2       /*  Test for write permission   */
    #define X_OK    1       /*  Test for execute permission */
    #define F_OK    0       /*  Test for existence of file  */
  #endif

#endif

//////////////////////////////////////////////////////////////////////////
#if (IS_WATCOM)
  #include <io.h>  //brings in STDERR/OUT/IN_FILENO, dup(), dup2(), close(), write(), etc...
  #include <process.h>  //brings in execlp() and _exit()
  #include <stdlib.h>   //brings in NULL
  #include <sys/stat.h>

  /* Define to 1 if ino_t is defined (possibly in sys/types.h) */
  #define HAVE_INO_T  1

  //NOT SURE IF O_NONBLOCK is OK here but at least the 0x0004 flag is not used by anything else...
  #define O_NONBLOCK 0x0004 /* Non-blocking I/O.  */
  //#define O_NDELAY   O_NONBLOCK

  //Prototypes for functions which we'll define in archive_windows.c
  extern unsigned int sleep (unsigned int seconds);

  #define lstat  _stat
#endif


//////////////////////////////////////////////////////////////////////////
#if !(HAVE_UINTPTR)
  typedef unsigned int *uintptr_t;
  #if defined(HAVE_UINTPTR)
    #undef  HAVE_UINTPTR
  #endif
  #define HAVE_UINTPTR 1
#endif

#if !defined(SSIZE_MAX)
  //#define _POSIX_SSIZE_MAX    32767
  #if defined(_POSIX_SSIZE_MAX)
    #define SSIZE_MAX   _POSIX_SSIZE_MAX
  #else
    #define SSIZE_MAX   ((ssize_t)((size_t)-1 >> 1))
  #endif
#endif

#if !(HAVE_FCNTL_FN)
  #define F_SETFL   4 /* Set file status flags.  */

  #if defined(HAVE_FCNTL_FN)
    #undef HAVE_FCNTL_FN
  #endif
  #define HAVE_FCNTL_FN 1
#endif

  #define  _S_IFLNK        0xA000    /* symbolic link */
  #if !defined(_S_IFMT)
    #define _S_IFMT  S_IFMT
  #endif

  #define  _S_ISLNK(m)     (((m) & _S_IFMT) == _S_IFLNK)
  #define   S_ISLNK(m)    _S_ISLNK(m)


/* Replacement for major/minor/makedev. */
#if !(MAJOR_IN_MKDEV) && !(MAJOR_IN_SYSMACROS)
  #define major(x) ((int)(0x00ff & ((x) >> 8)))
  #define minor(x) ((int)(0xffff00ff & (x)))
  #define makedev(maj,min) ((0xff00 & ((maj)<<8))|(0xffff00ff & (min)))
#endif

#define set_errno(val) ((errno)=val)


#endif /* CONFIG_H_INCLUDED */
