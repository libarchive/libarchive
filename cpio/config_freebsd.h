/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 * $FreeBSD: src/usr.bin/cpio/config_freebsd.h,v 1.3 2008/12/06 07:30:40 kientzle Exp $
 */

/* A hand-tooled configuration for FreeBSD. */

#include <sys/param.h>  /* __FreeBSD_version */

#define	HAVE_DIRENT_H 1
#define	HAVE_ERRNO_H 1
#define	HAVE_FCNTL_H 1
#define	HAVE_FUTIMES 1
#define	HAVE_GRP_H 1
#define	HAVE_LIBARCHIVE 1
#define	HAVE_LINK 1
#define	HAVE_LSTAT 1
#define	HAVE_LUTIMES 1
#define	HAVE_PWD_H 1
#define	HAVE_READLINK 1
#define	HAVE_STDARG_H 1
#define	HAVE_STDLIB_H 1
#define	HAVE_STRING_H 1
#define	HAVE_SYMLINK 1
#define	HAVE_SYS_CDEFS_H 1
#define	HAVE_SYS_STAT_H 1
#define	HAVE_SYS_TIME_H 1
#define	HAVE_TIME_H 1
#define	HAVE_UINTMAX_T 1
#define	HAVE_UNISTD_H 1
#define	HAVE_UNSIGNED_LONG_LONG 1
#define	HAVE_UTIME_H 1
#define	HAVE_UTIMES 1
#define	BUILD_7ZIP_FORMAT 1
#define	BUILD_AR_FORMAT 1
#define	BUILD_CAB_FORMAT 1
#define	BUILD_CPIO_FORMAT 1
#define	BUILD_ISO_FORMAT 1
#define	BUILD_LHA_FORMAT 1
#define	BUILD_MTREE_FORMAT 1
#define	BUILD_RAR_FORMAT 1
#define	BUILD_SHAR_FORMAT 1
#define	BUILD_XAR_FORMAT 1
#define	BUILD_ZIP_FORMAT 1

