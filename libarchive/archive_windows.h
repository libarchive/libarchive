/*-
 * Copyright (c) 2003-2006 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef LIBARCHIVE_NONPOSIX_H_INCLUDED
#define	LIBARCHIVE_NONPOSIX_H_INCLUDED

/* Start of configuration for native Win32  */

#include <errno.h>
#define	set_errno(val)	((errno)=val)
#include <io.h>
#include <stdlib.h>   //brings in NULL
#include <fcntl.h>
#include <sys/stat.h>
#include <process.h>
#include <direct.h>

//#define	EFTYPE 7

#if !defined(STDIN_FILENO)
#define STDIN_FILENO 0
#endif

#if !defined(STDOUT_FILENO)
#define STDOUT_FILENO 1
#endif

#if !defined(STDERR_FILENO)
#define STDERR_FILENO 2
#endif


#if defined(_MSC_VER)
/* TODO: Fix the code, don't suppress the warnings. */
#pragma warning(disable:4244)   /* 'conversion' conversion from 'type1' to 'type2', possible loss of data */
#pragma warning(disable:4146)   /* unary minus operator applied to unsigned type, result still unsigned */
//#pragma warning(disable:4996)	/* 'function': was declared deprecated */
//#pragma warning(disable:4267)   /* Conversion, possible loss of data */
#endif

#ifndef NULL
#ifdef  __cplusplus
#define	NULL    0
#else
#define	NULL    ((void *)0)
#endif
#endif

/* Replacement for major/minor/makedev. */
#define	major(x) ((int)(0x00ff & ((x) >> 8)))
#define	minor(x) ((int)(0xffff00ff & (x)))
#define	makedev(maj,min) ((0xff00 & ((maj)<<8))|(0xffff00ff & (min)))

/* Alias the Windows _function to the POSIX equivalent. */
#define	chdir		_chdir
#define	chmod		_chmod
#define	close		_close
//#define	fileno		_fileno
#define	fstat		_fstat
#define	lseek		_lseek
#define	lstat		_stat
#define	open		_open
#define	stat		_stat
#define	mkdir(d,m)	_mkdir(d)
#define	mktemp		_mktemp
#define	read		_read
#define	rmdir		_rmdir
#define	strdup		_strdup
#define	tzset		_tzset
#define	umask		_umask
#define	write		_write

#define	O_RDONLY	_O_RDONLY
#define	O_WRONLY	_O_WRONLY
#define	O_TRUNC		_O_TRUNC
#define	O_CREAT		_O_CREAT
#define	O_EXCL		_O_EXCL

#ifndef _S_IFIFO
  #define	_S_IFIFO        0010000   /* pipe */
#endif
#ifndef _S_IFCHR
  #define	_S_IFCHR        0020000   /* character special */
#endif
#ifndef _S_IFDIR
  #define	_S_IFDIR        0040000   /* directory */
#endif
#ifndef _S_IFBLK
  #define	_S_IFBLK        0060000   /* block special */
#endif
#ifndef _S_IFLNK
  #define	_S_IFLNK        0120000   /* symbolic link */
#endif
#ifndef _S_IFSOCK
  #define	_S_IFSOCK       0140000   /* socket */
#endif
#ifndef	_S_IFREG
  #define	_S_IFREG        0100000   /* regular */
#endif
#ifndef	_S_IFMT
  #define	_S_IFMT         0170000   /* file type mask */
#endif

#define	S_IFIFO     _S_IFIFO
//#define	S_IFCHR  _S_IFCHR
//#define	S_IFDIR  _S_IFDIR
#define	S_IFBLK     _S_IFBLK
#define	S_IFLNK     _S_IFLNK
#define	S_IFSOCK    _S_IFSOCK
//#define	S_IFREG  _S_IFREG
//#define	S_IFMT   _S_IFMT

#define	S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)	/* block special */
#define	S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)	/* fifo or socket */
#define	S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)	/* char special */
#define	S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)	/* directory */
#define	S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)	/* regular file */
#define	S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK) /* Symbolic link */
#define	S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK) /* Socket */

#define	_S_ISUID        0004000   /* set user id on execution */
#define	_S_ISGID        0002000   /* set group id on execution */
#define	_S_ISVTX        0001000   /* save swapped text even after use */

#define	S_ISUID        _S_ISUID
#define	S_ISGID        _S_ISGID
#define	S_ISVTX        _S_ISVTX

#define	_S_IRWXU	     (_S_IREAD | _S_IWRITE | _S_IEXEC)
#define	_S_IXUSR	     _S_IEXEC  /* read permission, user */
#define	_S_IWUSR	     _S_IWRITE /* write permission, user */
#define	_S_IRUSR	     _S_IREAD  /* execute/search permission, user */
#define	_S_IRWXG        (_S_IRWXU >> 3)
#define	_S_IXGRP        (_S_IXUSR >> 3) /* read permission, group */
#define	_S_IWGRP        (_S_IWUSR >> 3) /* write permission, group */
#define	_S_IRGRP        (_S_IRUSR >> 3) /* execute/search permission, group */
#define	_S_IRWXO        (_S_IRWXG >> 3) 
#define	_S_IXOTH        (_S_IXGRP >> 3) /* read permission, other */
#define	_S_IWOTH        (_S_IWGRP >> 3) /* write permission, other */
#define	_S_IROTH        (_S_IRGRP  >> 3) /* execute/search permission, other */

#define	S_IRWXU	     _S_IRWXU
#define	S_IXUSR	     _S_IXUSR
#define	S_IWUSR	     _S_IWUSR
#define	S_IRUSR	     _S_IRUSR
#define	S_IRWXG        _S_IRWXG
#define	S_IXGRP        _S_IXGRP
#define	S_IWGRP        _S_IWGRP
#define	S_IRGRP        _S_IRGRP
#define	S_IRWXO        _S_IRWXO
#define	S_IXOTH        _S_IXOTH
#define	S_IWOTH        _S_IWOTH
#define	S_IROTH        _S_IROTH

#define	F_DUPFD	  	0	/* Duplicate file descriptor.  */
#define	F_GETFD		1	/* Get file descriptor flags.  */
#define	F_SETFD		2	/* Set file descriptor flags.  */
#define	F_GETFL		3	/* Get file status flags.  */
#define	F_SETFL		4	/* Set file status flags.  */
#define	F_GETOWN		5	/* Get owner (receiver of SIGIO).  */
#define	F_SETOWN		6	/* Set owner (receiver of SIGIO).  */
#define	F_GETLK		7	/* Get record locking info.  */
#define	F_SETLK		8	/* Set record locking info (non-blocking).  */
#define	F_SETLKW		9	/* Set record locking info (blocking).  */

/* XXX missing */
#define	F_GETLK64	7	/* Get record locking info.  */
#define	F_SETLK64	8	/* Set record locking info (non-blocking).  */
#define	F_SETLKW64	9	/* Set record locking info (blocking).  */

/* File descriptor flags used with F_GETFD and F_SETFD.  */
#define	FD_CLOEXEC	1	/* Close on exec.  */

//NOT SURE IF O_NONBLOCK is OK here but at least the 0x0004 flag is not used by anything else...
#define	O_NONBLOCK 0x0004 /* Non-blocking I/O.  */
//#define	O_NDELAY   O_NONBLOCK

/* Symbolic constants for the access() function */
#if !defined(F_OK)
    #define	R_OK    4       /*  Test for read permission    */
    #define	W_OK    2       /*  Test for write permission   */
    #define	X_OK    1       /*  Test for execute permission */
    #define	F_OK    0       /*  Test for existence of file  */
#endif


#ifdef _LARGEFILE_SOURCE
# define __USE_LARGEFILE 1		/* declare fseeko and ftello */
#endif

#if defined _FILE_OFFSET_BITS && _FILE_OFFSET_BITS == 64
# define __USE_FILE_OFFSET64  1	/* replace 32-bit functions by 64-bit ones */
#endif

#if __USE_LARGEFILE && __USE_FILE_OFFSET64
/* replace stat and seek by their large-file equivalents */
#undef	stat
#define	stat		_stati64
#undef	fstat
#define	fstat	_fstati64

#undef	lseek
#define	lseek       _lseeki64
#define	lseek64     _lseeki64
#define	tell        _telli64
#define	tell64      _telli64

#ifdef __MINGW32__
# define fseek      fseeko64
# define fseeko     fseeko64
# define ftell      ftello64
# define ftello     ftello64
# define ftell64    ftello64
#endif /* __MINGW32__ */
#endif /* LARGE_FILES */

/* End of Win32 definitions. */

#ifdef __cplusplus
extern "C" {
#endif

extern int link (const char *from, const char *to);
extern int symlink (const char *from, const char *to);

#ifdef __cplusplus
}
#endif

#endif /* LIBARCHIVE_NONPOSIX_H_INCLUDED  */
