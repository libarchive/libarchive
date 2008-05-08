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
#define LIBARCHIVE_NONPOSIX_H_INCLUDED

/* Start of configuration for native Win32  */

#include <errno.h>
#define set_errno(val)	((errno)=val)
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <process.h>
#include <direct.h>

#define	EFTYPE 7
#define	STDIN_FILENO 0
#define	STDOUT_FILENO 1
#define	STDERR_FILENO 2

/* TODO: Fix the code, don't suppress the warnings. */
#pragma warning(disable:4244)   /* 'conversion' conversion from 'type1' to 'type2', possible loss of data */
#pragma warning(disable:4146)   /* unary minus operator applied to unsigned type, result still unsigned */
#pragma warning(disable:4996)	/* 'function': was declared deprecated */
#pragma warning(disable:4267)   /* Conversion, possible loss of data */

/* Basic definitions for system and integer types. */
#ifndef _SSIZE_T_
# define SSIZE_MAX LONG_MAX
#define _SSIZE_T_
#endif /* _SSIZE_T_ */

#ifndef NULL
#ifdef  __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

/* Replacement for major/minor/makedev. */
#define	major(x) ((int)(0x00ff & ((x) >> 8)))
#define	minor(x) ((int)(0xffff00ff & (x)))
#define	makedev(maj,min) ((0xff00 & ((maj)<<8))|(0xffff00ff & (min)))

#define	EFTYPE 7
#ifndef STDERR_FILENO
#define	STDERR_FILENO 2
#endif /* STDERR_FILENO  */

/* Alias the Windows _function to the POSIX equivalent. */
#define	chdir		_chdir
#define	chmod		_chmod
#define	close		_close
#define	fileno		_fileno
#define	fstat		_fstat
#define	lseek		_lseek
#define	open			_open
#define	stat			_stat
#define	mkdir(d,m)	_mkdir(d)
#define	mktemp		_mktemp
#define	read			_read
#define	rmdir		_rmdir
#define	strdup		_strdup
#define	tzset		_tzset
#define	umask		_umask
#define	write		_write

#define O_RDONLY	_O_RDONLY
#define	O_WRONLY	_O_WRONLY
#define	O_TRUNC		_O_TRUNC
#define	O_CREAT		_O_CREAT
#define	O_EXCL		_O_EXCL

#define	S_ISUID	0004000
#define	S_ISGID 0002000
#define	S_ISVTX 0001000

#define	S_IFMT	0170000
#define	S_IFDIR _S_IFDIR
#define	S_IFREG	_S_IFREG

#define	S_ISDIR(m)	(((m) & 0170000) == _S_IFDIR)	/* directory */
#define	S_ISCHR(m)	(((m) & 0170000) == _S_IFCHR)	/* char special */
#define	S_ISREG(m)	(((m) & 0170000) == _S_IFREG)	/* regular file */

/* Windows doesn't have the following, so they're trivial. */
#define	S_ISBLK(m)	(0)	/* block special */
#define	S_ISFIFO(m)	(0)	/* fifo or socket */
#define	S_ISLNK(m)    (0)	/* Symbolic link */
#define	S_ISSOCK(m)   (0)	/* Socket */

/* replace stat and seek by their large-file equivalents */
#undef	stat
#define   stat		_stati64
#undef	fstat
#define   fstat	_fstati64

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
