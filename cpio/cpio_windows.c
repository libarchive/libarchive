/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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
 * $FreeBSD$
 */

#if defined(_WIN32) && !defined(__CYGWIN__)

#include "cpio_platform.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stddef.h>
#include <sys/utime.h>
#include <sys/stat.h>
#include <process.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <sddl.h>

#include "cpio.h"
#include "err.h"

#define EPOC_TIME	(116444736000000000ULL)

struct ustat {
	int64_t		st_atime;
	uint32_t	st_atime_nsec;
	int64_t		st_ctime;
	uint32_t	st_ctime_nsec;
	int64_t		st_mtime;
	uint32_t	st_mtime_nsec;
	gid_t		st_gid;
	/* 64bits ino */
	int64_t		st_ino;
	mode_t		st_mode;
	uint32_t	st_nlink;
	uint64_t	st_size;
	uid_t		st_uid;
	dev_t		st_dev;
	dev_t		st_rdev;
};

static void cpio_dosmaperr(unsigned long);

/* Transform 64-bits ino into 32-bits by hashing.
 * You do not forget that really unique number size is 64-bits.
 */
#define INOSIZE (8*sizeof(ino_t)) /* 32 */
static __inline ino_t
getino(struct ustat *ub)
{
	ULARGE_INTEGER ino64;

	ino64.QuadPart = ub->st_ino;
	/* I don't know this hashing is correct way */
	return (ino_t)(ino64.LowPart ^ (ino64.LowPart >> INOSIZE));
}

/*
 * Prepend "\\?\" to the path name and convert it to unicode to permit
 * an extended-length path for a maximum total path length of 32767
 * characters.
 * see also http://msdn.microsoft.com/en-us/library/aa365247.aspx
 */
static wchar_t *
permissive_name(const char *name)
{
	wchar_t *wn, *wnp;
	wchar_t *ws, *wsp;
	size_t l, len, slen, alloclen;
	int unc;

	len = strlen(name);
	wn = malloc((len + 1) * sizeof(wchar_t));
	if (wn == NULL)
		return (NULL);
	l = MultiByteToWideChar(CP_ACP, 0, name, len, wn, len);
	if (l == 0) {
		free(wn);
		return (NULL);
	}
	wn[l] = L'\0';

	/* Get a full path names */
	l = GetFullPathNameW(wn, 0, NULL, NULL);
	if (l == 0) {
		free(wn);
		return (NULL);
	}
	wnp = malloc(l * sizeof(wchar_t));
	if (wnp == NULL) {
		free(wn);
		return (NULL);
	}
	len = GetFullPathNameW(wn, l, wnp, NULL);
	free(wn);
	wn = wnp;

	if (wnp[0] == L'\\' && wnp[1] == L'\\' &&
	    wnp[2] == L'?' && wnp[3] == L'\\')
		/* We have already permissive names. */
		return (wn);

	if (wnp[0] == L'\\' && wnp[1] == L'\\' &&
		wnp[2] == L'.' && wnp[3] == L'\\') {
		/* Device names */
		if (((wnp[4] >= L'a' && wnp[4] <= L'z') ||
		     (wnp[4] >= L'A' && wnp[4] <= L'Z')) &&
		    wnp[5] == L':' && wnp[6] == L'\\')
			wnp[2] = L'?';/* Not device names. */
		return (wn);
	}

	unc = 0;
	if (wnp[0] == L'\\' && wnp[1] == L'\\' && wnp[2] != L'\\') {
		wchar_t *p = &wnp[2];

		/* Skip server-name letters. */
		while (*p != L'\\' && *p != L'\0')
			++p;
		if (*p == L'\\') {
			wchar_t *rp = ++p;
			/* Skip share-name letters. */
			while (*p != L'\\' && *p != L'\0')
				++p;
			if (*p == L'\\' && p != rp) {
				/* Now, match patterns such as
				 * "\\server-name\share-name\" */
				wnp += 2;
				len -= 2;
				unc = 1;
			}
		}
	}

	alloclen = slen = 4 + (unc * 4) + len + 1;
	ws = wsp = malloc(slen * sizeof(wchar_t));
	if (ws == NULL) {
		free(wn);
		return (NULL);
	}
	/* prepend "\\?\" */
	wcsncpy(wsp, L"\\\\?\\", 4);
	wsp += 4;
	slen -= 4;
	if (unc) {
		/* append "UNC\" ---> "\\?\UNC\" */
		wcsncpy(wsp, L"UNC\\", 4);
		wsp += 4;
		slen -= 4;
	}
	wcsncpy(wsp, wnp, slen);
	free(wn);
	ws[alloclen - 1] = L'\0';
	return (ws);
}

static HANDLE
la_CreateFile(const char *path, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	wchar_t *wpath;
	HANDLE handle;

	handle = CreateFileA(path, dwDesiredAccess, dwShareMode,
	    lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
	    hTemplateFile);
	if (handle != INVALID_HANDLE_VALUE)
		return (handle);
	if (GetLastError() != ERROR_PATH_NOT_FOUND)
		return (handle);
	wpath = permissive_name(path);
	if (wpath == NULL)
		return (handle);
	handle = CreateFileW(wpath, dwDesiredAccess, dwShareMode,
	    lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
	    hTemplateFile);
	free(wpath);
	return (handle);
}

#define WINTIME(sec, usec)	((Int32x32To64(sec, 10000000) + EPOC_TIME) + (usec * 10))
static int
__hutimes(HANDLE handle, const struct __timeval *times)
{
	ULARGE_INTEGER wintm;
	FILETIME fatime, fmtime;

	wintm.QuadPart = WINTIME(times[0].tv_sec, times[0].tv_usec);
	fatime.dwLowDateTime = wintm.LowPart;
	fatime.dwHighDateTime = wintm.HighPart;
	wintm.QuadPart = WINTIME(times[1].tv_sec, times[1].tv_usec);
	fmtime.dwLowDateTime = wintm.LowPart;
	fmtime.dwHighDateTime = wintm.HighPart;
	if (SetFileTime(handle, NULL, &fatime, &fmtime) == 0) {
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

int
futimes(int fd, const struct __timeval *times)
{

	return (__hutimes((HANDLE)_get_osfhandle(fd), times));
}

int
utimes(const char *name, const struct __timeval *times)
{
	int ret;
	HANDLE handle;

	handle = la_CreateFile(name, GENERIC_READ | GENERIC_WRITE,
	    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
	    FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		cpio_dosmaperr(GetLastError());
		return (-1);
	}
	ret = __hutimes(handle, times);
	CloseHandle(handle);
	return (ret);
}

int
la_chdir(const char *path)
{
	wchar_t *ws;
	int r;

	r = SetCurrentDirectoryA(path);
	if (r == 0) {
		if (GetLastError() != ERROR_FILE_NOT_FOUND) {
			cpio_dosmaperr(GetLastError());
			return (-1);
		}
	} else
		return (0);
	ws = permissive_name(path);
	if (ws == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = SetCurrentDirectoryW(ws);
	free(ws);
	if (r == 0) {
		cpio_dosmaperr(GetLastError());
		return (-1);
	}
	return (0);
}

int
la_open(const char *path, int flags, ...)
{
	va_list ap;
	wchar_t *ws;
	int r, pmode;
	DWORD attr;

	va_start(ap, flags);
	pmode = va_arg(ap, int);
	va_end(ap);
	ws = NULL;
	if ((flags & ~_O_BINARY) == _O_RDONLY) {
		/*
		 * When we open a directory, _open function returns 
		 * "Permission denied" error.
		 */
		attr = GetFileAttributesA(path);
		if (attr == -1 && GetLastError() == ERROR_PATH_NOT_FOUND) {
			ws = permissive_name(path);
			if (ws == NULL) {
				errno = EINVAL;
				return (-1);
			}
			attr = GetFileAttributesW(ws);
		}
		if (attr == -1) {
			cpio_dosmaperr(GetLastError());
			free(ws);
			return (-1);
		}
		if (attr & FILE_ATTRIBUTE_DIRECTORY) {
			HANDLE handle;

			if (ws != NULL)
				handle = CreateFileW(ws, 0, 0, NULL,
				    OPEN_EXISTING,
				    FILE_FLAG_BACKUP_SEMANTICS |
				    FILE_ATTRIBUTE_READONLY,
					NULL);
			else
				handle = CreateFileA(path, 0, 0, NULL,
				    OPEN_EXISTING,
				    FILE_FLAG_BACKUP_SEMANTICS |
				    FILE_ATTRIBUTE_READONLY,
					NULL);
			free(ws);
			if (handle == INVALID_HANDLE_VALUE) {
				cpio_dosmaperr(GetLastError());
				return (-1);
			}
			r = _open_osfhandle((intptr_t)handle, _O_RDONLY);
			return (r);
		}
	}
	if (ws == NULL) {
		r = _open(path, flags, pmode);
		if (r < 0 && errno == EACCES && (flags & O_CREAT) != 0) {
			/* simular other POSIX system action to pass a test */
			attr = GetFileAttributesA(path);
			if (attr == -1)
				cpio_dosmaperr(GetLastError());
			else if (attr & FILE_ATTRIBUTE_DIRECTORY)
				errno = EISDIR;
			else
				errno = EACCES;
			return (-1);
		}
		if (r >= 0 || errno != ENOENT)
			return (r);
		ws = permissive_name(path);
		if (ws == NULL) {
			errno = EINVAL;
			return (-1);
		}
	}
	r = _wopen(ws, flags, pmode);
	if (r < 0 && errno == EACCES && (flags & O_CREAT) != 0) {
		/* simular other POSIX system action to pass a test */
		attr = GetFileAttributesW(ws);
		if (attr == -1)
			cpio_dosmaperr(GetLastError());
		else if (attr & FILE_ATTRIBUTE_DIRECTORY)
			errno = EISDIR;
		else
			errno = EACCES;
	}
	free(ws);
	return (r);
}

ssize_t
la_read(int fd, void *buf, size_t nbytes)
{
	HANDLE handle;
	DWORD bytes_read, lasterr;
	int r;

#ifdef _WIN64
	if (nbytes > UINT32_MAX)
		nbytes = UINT32_MAX;
#endif
	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	handle = (HANDLE)_get_osfhandle(fd);
	if (GetFileType(handle) == FILE_TYPE_PIPE) {
		DWORD sta;
		if (GetNamedPipeHandleState(
		    handle, &sta, NULL, NULL, NULL, NULL, 0) != 0 &&
		    (sta & PIPE_NOWAIT) == 0) {
			DWORD avail = -1;
			int cnt = 3;

			while (PeekNamedPipe(
			    handle, NULL, 0, NULL, &avail, NULL) != 0 &&
			    avail == 0 && --cnt)
				Sleep(100);
			if (avail == 0)
				return (0);
		}
	}
	r = ReadFile(handle, buf, (uint32_t)nbytes,
	    &bytes_read, NULL);
	if (r == 0) {
		lasterr = GetLastError();
		if (lasterr == ERROR_NO_DATA) {
			errno = EAGAIN;
			return (-1);
		}
		if (lasterr == ERROR_BROKEN_PIPE)
			return (0);
		if (lasterr == ERROR_ACCESS_DENIED)
			errno = EBADF;
		else
			cpio_dosmaperr(lasterr);
		return (-1);
	}
	return ((ssize_t)bytes_read);
}

/* Convert Windows FILETIME to UTC */
__inline static void
fileTimeToUTC(const FILETIME *filetime, time_t *time, long *ns)
{
	ULARGE_INTEGER utc;

	utc.HighPart = filetime->dwHighDateTime;
	utc.LowPart  = filetime->dwLowDateTime;
	if (utc.QuadPart >= EPOC_TIME) {
		utc.QuadPart -= EPOC_TIME;
		*time = (time_t)(utc.QuadPart / 10000000);	/* milli seconds base */
		*ns = (long)(utc.QuadPart % 10000000) * 100;/* nano seconds base */
	} else {
		*time = 0;
		*ns = 0;
	}
}

ssize_t
la_write(int fd, const void *buf, size_t nbytes)
{
	DWORD bytes_written;

#ifdef _WIN64
	if (nbytes > UINT32_MAX)
		nbytes = UINT32_MAX;
#endif
	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	if (!WriteFile((HANDLE)_get_osfhandle(fd), buf, (uint32_t)nbytes,
	    &bytes_written, NULL)) {
		DWORD lasterr;

		lasterr = GetLastError();
		if (lasterr == ERROR_ACCESS_DENIED)
			errno = EBADF;
		else
			cpio_dosmaperr(lasterr);
		return (-1);
	}
	return (bytes_written);
}

//#endif
/*
 * The following function was modified from PostgreSQL sources and is
 * subject to the copyright below.
 */
/*-------------------------------------------------------------------------
 *
 * win32error.c
 *	  Map win32 error codes to errno values
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/port/win32error.c,v 1.4 2008/01/01 19:46:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
PostgreSQL Database Management System
(formerly known as Postgres, then as Postgres95)

Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group

Portions Copyright (c) 1994, The Regents of the University of California

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose, without fee, and without a written agreement
is hereby granted, provided that the above copyright notice and this
paragraph and the following two paragraphs appear in all copies.

IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATIONS TO
PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/

static const struct {
	DWORD		winerr;
	int		doserr;
} doserrors[] =
{
	{	ERROR_INVALID_FUNCTION, EINVAL	},
	{	ERROR_FILE_NOT_FOUND, ENOENT	},
	{	ERROR_PATH_NOT_FOUND, ENOENT	},
	{	ERROR_TOO_MANY_OPEN_FILES, EMFILE	},
	{	ERROR_ACCESS_DENIED, EACCES	},
	{	ERROR_INVALID_HANDLE, EBADF	},
	{	ERROR_ARENA_TRASHED, ENOMEM	},
	{	ERROR_NOT_ENOUGH_MEMORY, ENOMEM	},
	{	ERROR_INVALID_BLOCK, ENOMEM	},
	{	ERROR_BAD_ENVIRONMENT, E2BIG	},
	{	ERROR_BAD_FORMAT, ENOEXEC	},
	{	ERROR_INVALID_ACCESS, EINVAL	},
	{	ERROR_INVALID_DATA, EINVAL	},
	{	ERROR_INVALID_DRIVE, ENOENT	},
	{	ERROR_CURRENT_DIRECTORY, EACCES	},
	{	ERROR_NOT_SAME_DEVICE, EXDEV	},
	{	ERROR_NO_MORE_FILES, ENOENT	},
	{	ERROR_LOCK_VIOLATION, EACCES	},
	{	ERROR_SHARING_VIOLATION, EACCES	},
	{	ERROR_BAD_NETPATH, ENOENT	},
	{	ERROR_NETWORK_ACCESS_DENIED, EACCES	},
	{	ERROR_BAD_NET_NAME, ENOENT	},
	{	ERROR_FILE_EXISTS, EEXIST	},
	{	ERROR_CANNOT_MAKE, EACCES	},
	{	ERROR_FAIL_I24, EACCES	},
	{	ERROR_INVALID_PARAMETER, EINVAL	},
	{	ERROR_NO_PROC_SLOTS, EAGAIN	},
	{	ERROR_DRIVE_LOCKED, EACCES	},
	{	ERROR_BROKEN_PIPE, EPIPE	},
	{	ERROR_DISK_FULL, ENOSPC	},
	{	ERROR_INVALID_TARGET_HANDLE, EBADF	},
	{	ERROR_INVALID_HANDLE, EINVAL	},
	{	ERROR_WAIT_NO_CHILDREN, ECHILD	},
	{	ERROR_CHILD_NOT_COMPLETE, ECHILD	},
	{	ERROR_DIRECT_ACCESS_HANDLE, EBADF	},
	{	ERROR_NEGATIVE_SEEK, EINVAL	},
	{	ERROR_SEEK_ON_DEVICE, EACCES	},
	{	ERROR_DIR_NOT_EMPTY, ENOTEMPTY	},
	{	ERROR_NOT_LOCKED, EACCES	},
	{	ERROR_BAD_PATHNAME, ENOENT	},
	{	ERROR_MAX_THRDS_REACHED, EAGAIN	},
	{	ERROR_LOCK_FAILED, EACCES	},
	{	ERROR_ALREADY_EXISTS, EEXIST	},
	{	ERROR_FILENAME_EXCED_RANGE, ENOENT	},
	{	ERROR_NESTING_NOT_ALLOWED, EAGAIN	},
	{	ERROR_NOT_ENOUGH_QUOTA, ENOMEM	}
};

static void
cpio_dosmaperr(unsigned long e)
{
	int			i;

	if (e == 0)	{
		errno = 0;
		return;
	}

	for (i = 0; i < sizeof(doserrors); i++) {
		if (doserrors[i].winerr == e) {
			errno = doserrors[i].doserr;
			return;
		}
	}

	/* fprintf(stderr, "unrecognized win32 error code: %lu", e); */
	errno = EINVAL;
	return;
}
#endif
