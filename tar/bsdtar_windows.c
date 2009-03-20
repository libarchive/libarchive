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
#define _WIN32_WINNT 0x0500
#define WINVER       0x0500

#include "bsdtar_platform.h"
#include <errno.h>
#include <stddef.h>
#include <sys/utime.h>
#include <sys/stat.h>
#include <process.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <sddl.h>

#include "bsdtar.h"

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

struct __DIR {
	HANDLE			handle;
	WIN32_FIND_DATAW	fileData;
	struct dirent		de;
	int			first;
	BOOL			finished;
};

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
	return (ino64.LowPart ^ (ino64.LowPart >> INOSIZE));
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
	size_t l, len, slen;
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

	slen = 4 + (unc * 4) + len + 1;
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
	wcsncpy_s(wsp, slen, wnp, _TRUNCATE);
	free(wn);
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

static size_t
wequallen(const wchar_t *s1, const wchar_t *s2)
{
	size_t i = 0;

	while (*s1 != L'\0' && *s2 != L'\0' && *s1 == *s2) {
		++s1; ++s2; ++i;
	}
	return (i);
}

/* Check that path1 and path2 can be hard-linked by each other.
 * Both arguments must be made by permissive_name function. 
 */
static int
canHardLinkW(const wchar_t *path1, const wchar_t *path2)
{
	wchar_t root[MAX_PATH];
	wchar_t fs[32];
	const wchar_t *s;
	int r;

	r = wequallen(path1, path2);
	/* Is volume-name the same? */
	if (r < 7)
		return (0);
	if (wcsncmp(path1, L"\\\\?\\UNC\\", 8) == 0) {
		int len;

		s = path1 + 8;
		if (*s == L'\\')
			return (0);
		/*         012345678
		 * Name : "\\?\UNC\Server\Share\"
		 *                       ^ search
		 */
		s = wcschr(++s, L'\\');
		if (s == NULL)
			return (0);
		if (*++s == L'\\')
			return (0);
		/*         012345678
		 * Name : "\\?\UNC\Server\Share\"
		 *                             ^ search
		 */
		s = wcschr(++s, L'\\');
		if (s == NULL)
			return (0);
		s++;
		/*         012345678
		 * Name : "\\?\UNC\Server\Share\xxxx"
		 *                 ^--- len ----^
		 */
		len = (int)(s - path1 - 8);
		/* Is volume-name the same? */
		if (r < len + 8)
			return (0);
		/* Is volume-name too long? */
		if (sizeof(root) -3 < len)
			return (0);
		root[0] = root[1] = L'\\';
		wcsncpy(root + 2, path1 + 8 , len);
		/* root : "\\Server\Share\" */
		root[2 + len] = L'\0';
	} else if (wcsncmp(path1, L"\\\\?\\", 4) == 0) {
		s = path1 + 4;
		if ((!iswalpha(*s)) || s[1] != L':' || s[2] != L'\\')
			return (0);
		wcsncpy(root, path1 + 4, 3);
		root[3] = L'\0';
	} else
		return (0);
	if (!GetVolumeInformationW(root, NULL, 0, NULL, NULL, NULL, fs, sizeof(fs)))
		return (0);
	if (wcscmp(fs, L"NTFS") == 0)
		return (1);
	else
		return (0);
}

/* Make a link to src called dst.  */
static int
__link(const char *src, const char *dst, int sym)
{
	wchar_t *wsrc, *wdst;
	int res, retval;
	DWORD attr;

	if (src == NULL || dst == NULL) {
		set_errno (EINVAL);
		return -1;
	}

	wsrc = permissive_name(src);
	wdst = permissive_name(dst);
	if (wsrc == NULL || wdst == NULL) {
		if (wsrc != NULL)
			free(wsrc);
		if (wdst != NULL)
			free(wdst);
		set_errno (EINVAL);
		return -1;
	}

	if ((attr = GetFileAttributesW(wsrc)) != -1) {
		if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			errno = EPERM;
			retval = -1;
			goto exit;
		}
		if (!sym && canHardLinkW(wsrc, wdst))
			res = CreateHardLinkW(wdst, wsrc, NULL);
		else
			res = CopyFileW(wsrc, wdst, FALSE);
	} else {
		/* wsrc does not exist; try src prepend it with the dirname of wdst */
		wchar_t *wnewsrc, *slash;
		int i, n, slen, wlen;

		if (strlen(src) >= 3 && isalpha((unsigned char)src[0]) &&
		    src[1] == ':' && src[2] == '\\') {
			/* Original src name is already full-path */
			retval = -1;
			goto exit;
		}
		if (src[0] == '\\') {
			/* Original src name is almost full-path
			 * (maybe src name is without drive) */
			retval = -1;
			goto exit;
		}

		wnewsrc = malloc ((wcslen(wsrc) + wcslen(wdst) + 1) * sizeof(wchar_t));
		if (wnewsrc == NULL) {
			errno = ENOMEM;
			retval = -1;
			goto exit;
		}
		/* Copying a dirname of wdst */
		wcscpy(wnewsrc, wdst);
		slash = wcsrchr(wnewsrc, L'\\');
		if (slash != NULL)
			*++slash = L'\0';
		else
			wcscat(wnewsrc, L"\\");
		/* Converting multi-byte src to wide-char src */
		wlen = wcslen(wsrc);
		slen = strlen(src);
		n = MultiByteToWideChar(CP_ACP, 0, src, slen, wsrc, slen);
		if (n == 0) {
			free (wnewsrc);
			retval = -1;
			goto exit;
		}
		for (i = 0; i < n; i++)
			if (wsrc[i] == L'/')
				wsrc[i] = L'\\';
		wcsncat(wnewsrc, wsrc, n);
		/* Check again */
		attr = GetFileAttributesW(wnewsrc);
		if (attr == -1 || (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (attr == -1)
				_dosmaperr(GetLastError());
			else
				errno = EPERM;
			free (wnewsrc);
			retval = -1;
			goto exit;
		}
		if (!sym && canHardLinkW(wnewsrc, wdst))
			res = CreateHardLinkW(wdst, wnewsrc, NULL);
		else
			res = CopyFileW(wnewsrc, wdst, FALSE);
		free (wnewsrc);
	}
	if (res == 0) {
		_dosmaperr(GetLastError());
		retval = -1;
	} else
		retval = 0;
exit:
	free(wsrc);
	free(wdst);
	return (retval);
}

/* Make a hard link to src called dst.  */
int
link(const char *src, const char *dst)
{
	return __link (src, dst, 0);
}

/* Make a symbolic link to FROM called TO.  */
int symlink (from, to)
     const char *from;
     const char *to;
{
	return __link (from, to, 1);
}

int
ftruncate(int fd, off_t length)
{
	LARGE_INTEGER distance;
	HANDLE handle;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	handle = (HANDLE)_get_osfhandle(fd);
	if (GetFileType(handle) != FILE_TYPE_DISK) {
		errno = EBADF;
		return (-1);
	}
	distance.QuadPart = length;
	if (!SetFilePointerEx(handle, distance, NULL, FILE_BEGIN)) {
		_dosmaperr(GetLastError());
		return (-1);
	}
	if (!SetEndOfFile(handle)) {
		_dosmaperr(GetLastError());
		return (-1);
	}
	return (0);
}

DIR *
__opendir(const char *path, int ff)
{
	DIR *dir;
	wchar_t *wpath, *wfname;
	size_t wlen;

	dir = malloc(sizeof(*dir));
	if (dir == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	wpath = permissive_name(path);
	if (wpath == NULL) {
		errno = EINVAL;
		free(dir);
		return (NULL);
	}
	if (ff) {
		wfname = wpath;
		wpath = NULL;
	} else {
		wlen = wcslen(wpath);
		wfname = malloc((wlen + 3) * sizeof(wchar_t));
		if (wfname == NULL) {
			errno = ENOMEM;
			free(dir);
			free(wpath);
			return (NULL);
		}
		wcscpy(wfname, wpath);
		wcscat(wfname, L"\\*");
		free(wpath);
	}

	dir->handle = FindFirstFileW(wfname, &dir->fileData);
	if (dir->handle == INVALID_HANDLE_VALUE) {
		_dosmaperr(GetLastError());
		free(dir);
		free(wfname);
		return (NULL);
	}
	dir->first = 1;
	dir->finished = FALSE;
	free(wfname);

	return (dir);
}

static DIR *
opendir_findfile(const char *path)
{
	return (__opendir(path, 1));
}

DIR *
opendir(const char *path)
{
	return (__opendir(path, 0));
}

struct dirent *
readdir(DIR *dirp)
{
	size_t len;

	while (!dirp->finished) {
		if (!dirp->first && !FindNextFileW(dirp->handle, &dirp->fileData)) {
			if (GetLastError() != ERROR_NO_MORE_FILES)
				_dosmaperr(GetLastError());
			dirp->finished = TRUE;
			break;
		}
		dirp->first = 0;
		len = wcstombs(dirp->de.d_name, dirp->fileData.cFileName,
		    sizeof(dirp->de.d_name) -1);
		if (len == -1) {
			errno = EINVAL;
			dirp->finished = TRUE;
			break;
		}
		dirp->de.d_name[len] = '\0';
		dirp->de.d_nameln = strlen(dirp->de.d_name);
		return (&dirp->de);
	}

	return (NULL);
}

int
closedir(DIR *dirp)
{
	BOOL ret;

	ret = FindClose(dirp->handle);
	free(dirp);
	if (ret == 0) {
		_dosmaperr(GetLastError());
		return (-1);
	}
	return (0);
}

int
la_chdir(const char *path)
{
	wchar_t *ws;
	int r;

	r = SetCurrentDirectoryA(path);
	if (r == 0) {
		if (GetLastError() != ERROR_FILE_NOT_FOUND) {
			_dosmaperr(GetLastError());
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
		_dosmaperr(GetLastError());
		return (-1);
	}
	return (0);
}

__int64
la_lseek(int fd, __int64 offset, int whence)
{
	LARGE_INTEGER distance;
	LARGE_INTEGER newpointer;
	HANDLE handle;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	handle = (HANDLE)_get_osfhandle(fd);
	if (GetFileType(handle) != FILE_TYPE_DISK) {
		errno = EBADF;
		return (-1);
	}
	distance.QuadPart = offset;
	if (!SetFilePointerEx(handle, distance, &newpointer, whence)) {
		DWORD lasterr;

		lasterr = GetLastError();
		if (lasterr == ERROR_BROKEN_PIPE)
			return (0);
		if (lasterr == ERROR_ACCESS_DENIED)
			errno = EBADF;
		else
			_dosmaperr(lasterr);
		return (-1);
	}
	return (newpointer.QuadPart);
}

int
la_mkdir(const char *path, mode_t mode)
{
	wchar_t *ws;
	int r;

	(void)mode;/* UNUSED */
	r = CreateDirectoryA(path, NULL);
	if (r == 0) {
		DWORD lasterr = GetLastError();
		if (lasterr != ERROR_FILENAME_EXCED_RANGE &&
			lasterr != ERROR_PATH_NOT_FOUND) {
			_dosmaperr(GetLastError());
			return (-1);
		}
	} else
		return (0);
	ws = permissive_name(path);
	if (ws == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = CreateDirectoryW(ws, NULL);
	free(ws);
	if (r == 0) {
		_dosmaperr(GetLastError());
		return (-1);
	}
	return (0);
}

/* Windows' mbstowcs is differrent error handling from other unix mbstowcs.
 * That one is using MultiByteToWideChar function with MB_PRECOMPOSED and
 * MB_ERR_INVALID_CHARS flags.
 * This implements for only to pass libarchive_test.
 */
size_t
la_mbstowcs(wchar_t *wcstr, const char *mbstr, size_t nwchars)
{

	return (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS,
	    mbstr, (int)strlen(mbstr), wcstr,
	    (int)nwchars));
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
	if ((flags & ~O_BINARY) == O_RDONLY) {
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
			_dosmaperr(GetLastError());
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
				_dosmaperr(GetLastError());
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
				_dosmaperr(GetLastError());
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
			_dosmaperr(GetLastError());
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
			_dosmaperr(lasterr);
		return (-1);
	}
	return ((ssize_t)bytes_read);
}

/* Remove directory */
int
la_rmdir(const char *path)
{
	wchar_t *ws;
	int r;

	r = _rmdir(path);
	if (r >= 0 || errno != ENOENT)
		return (r);
	ws = permissive_name(path);
	if (ws == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = _wrmdir(ws);
	free(ws);
	return (r);
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

/* Stat by handle
 * Windows' stat() does not accept path which is added "\\?\" especially "?"
 * character.
 * It means we cannot access a long name path(which is longer than MAX_PATH).
 * So I've implemented simular Windows' stat() to access the long name path.
 * And I've added some feature.
 * 1. set st_ino by nFileIndexHigh and nFileIndexLow of
 *    BY_HANDLE_FILE_INFORMATION.
 * 2. set st_nlink by nNumberOfLinks of BY_HANDLE_FILE_INFORMATION.
 * 3. set st_dev by dwVolumeSerialNumber by BY_HANDLE_FILE_INFORMATION.
 */
static int
__hstat(HANDLE handle, struct ustat *st)
{
	BY_HANDLE_FILE_INFORMATION info;
	ULARGE_INTEGER ino64;
	DWORD ftype;
	mode_t mode;
	time_t time;
	long ns;

	switch (ftype = GetFileType(handle)) {
	case FILE_TYPE_UNKNOWN:
		errno = EBADF;
		return (-1);
	case FILE_TYPE_CHAR:
	case FILE_TYPE_PIPE:
		if (ftype == FILE_TYPE_CHAR) {
			st->st_mode = S_IFCHR;
			st->st_size = 0;
		} else {
			DWORD avail;

			st->st_mode = S_IFIFO;
			if (PeekNamedPipe(handle, NULL, 0, NULL, &avail, NULL))
				st->st_size = avail;
			else
				st->st_size = 0;
		}
		st->st_atime = 0;
		st->st_atime_nsec = 0;
		st->st_mtime = 0;
		st->st_mtime_nsec = 0;
		st->st_ctime = 0;
		st->st_ctime_nsec = 0;
		st->st_ino = 0;
		st->st_nlink = 1;
		st->st_uid = 0;
		st->st_gid = 0;
		st->st_rdev = 0;
		st->st_dev = 0;
		return (0);
	case FILE_TYPE_DISK:
		break;
	default:
		/* This ftype is undocumented type. */
		_dosmaperr(GetLastError());
		return (-1);
	}

	ZeroMemory(&info, sizeof(info));
	if (!GetFileInformationByHandle (handle, &info)) {
		_dosmaperr(GetLastError());
		return (-1);
	}

	mode = S_IRUSR | S_IRGRP | S_IROTH;
	if ((info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0)
		mode |= S_IWUSR | S_IWGRP | S_IWOTH;
	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
	else
		mode |= S_IFREG;
	st->st_mode = mode;
	
	fileTimeToUTC(&info.ftLastAccessTime, &time, &ns);
	st->st_atime = time; 
	st->st_atime_nsec = ns;
	fileTimeToUTC(&info.ftLastWriteTime, &time, &ns);
	st->st_mtime = time;
	st->st_mtime_nsec = ns;
	fileTimeToUTC(&info.ftCreationTime, &time, &ns);
	st->st_ctime = time;
	st->st_ctime_nsec = ns;
	st->st_size = 
	    ((int64_t)(info.nFileSizeHigh) * ((int64_t)MAXDWORD + 1))
		+ (int64_t)(info.nFileSizeLow);
#ifdef SIMULATE_WIN_STAT
	st->st_ino = 0;
	st->st_nlink = 1;
	st->st_dev = 0;
#else
	/* Getting FileIndex as i-node. We have to remove a sequence which
	 * is high-16-bits of nFileIndexHigh. */
	ino64.HighPart = info.nFileIndexHigh & 0x0000FFFFUL;
	ino64.LowPart  = info.nFileIndexLow;
	st->st_ino = ino64.QuadPart;
	st->st_nlink = info.nNumberOfLinks;
	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		++st->st_nlink;/* Add parent directory. */
	st->st_dev = info.dwVolumeSerialNumber;
#endif
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;
	return (0);
}

static void
copy_stat(struct stat *st, struct ustat *us)
{
	st->st_atime = us->st_atime;
	st->st_ctime = us->st_ctime;
	st->st_mtime = us->st_mtime;
	st->st_gid = us->st_gid;
	st->st_ino = getino(us);
	st->st_mode = us->st_mode;
	st->st_nlink = us->st_nlink;
	st->st_size = us->st_size;
	st->st_uid = us->st_uid;
	st->st_dev = us->st_dev;
	st->st_rdev = us->st_rdev;
}

int
la_fstat(int fd, struct stat *st)
{
	struct ustat u;
	int ret;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	ret = __hstat((HANDLE)_get_osfhandle(fd), &u);
	if (ret >= 0) {
		copy_stat(st, &u);
		if (u.st_mode & (S_IFCHR | S_IFIFO)) {
			st->st_dev = fd;
			st->st_rdev = fd;
		}
	}
	return (ret);
}

int
la_stat(const char *path, struct stat *st)
{
	HANDLE handle;
	struct ustat u;
	int ret;

	handle = la_CreateFile(path, 0, 0, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_READONLY,
		NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		_dosmaperr(GetLastError());
		return (-1);
	}
	ret = __hstat(handle, &u);
	CloseHandle(handle);
	if (ret >= 0) {
		char *p;

		copy_stat(st, &u);
		p = strrchr(path, '.');
		if (p != NULL && strlen(p) == 4) {
			char exttype[4];

			++ p;
			exttype[0] = toupper(*p++);
			exttype[1] = toupper(*p++);
			exttype[2] = toupper(*p++);
			exttype[3] = '\0';
			if (!strcmp(exttype, "EXE") || !strcmp(exttype, "CMD") ||
				!strcmp(exttype, "BAT") || !strcmp(exttype, "COM"))
				st->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
		}
	}
	return (ret);
}

ssize_t
la_write(int fd, const void *buf, size_t nbytes)
{
	uint32_t bytes_written;

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
			_dosmaperr(lasterr);
		return (-1);
	}
	return (bytes_written);
}

#ifndef LIST_H
static int
_is_privileged(HANDLE thandle, const char *sidlist[])
{
	TOKEN_USER *tuser;
	TOKEN_GROUPS  *tgrp;
	DWORD bytes;
	PSID psid;
	DWORD i, g;
	int member;

	psid = NULL;
	tuser = NULL;
	tgrp = NULL;
	member = 0;
	for (i = 0; sidlist[i] != NULL && member == 0; i++) {
		if (psid != NULL)
			LocalFree(psid);
		if (ConvertStringSidToSidA(sidlist[i], &psid) == 0) {
			errno = EPERM;
			return (-1);
		}
		if (tuser == NULL) {
			GetTokenInformation(thandle, TokenUser, NULL, 0, &bytes);
			tuser = malloc(bytes);
			if (tuser == NULL) {
				errno = ENOMEM;
				member = -1;
				break;
			}
			if (GetTokenInformation(thandle, TokenUser, tuser, bytes, &bytes) == 0) {
				errno = EPERM;
				member = -1;
				break;
			}
		}
		member = EqualSid(tuser->User.Sid, psid);
		if (member)
			break;
		if (tgrp == NULL) {
			GetTokenInformation(thandle, TokenGroups, NULL, 0, &bytes);
			tgrp = malloc(bytes);
			if (tgrp == NULL) {
				errno = ENOMEM;
				member = -1;
				break;
			}
			if (GetTokenInformation(thandle, TokenGroups, tgrp, bytes, &bytes) == 0) {
				errno = EPERM;
				member = -1;
				break;
			}
		}
		for (g = 0; g < tgrp->GroupCount; g++) {
			member = EqualSid(tgrp->Groups[g].Sid, psid);
			if (member)
				break;
		}
	}
	LocalFree(psid);
	free(tuser);
	free(tgrp);

	return (member);
}

int
bsdtar_is_privileged(struct bsdtar *bsdtar)
{
	HANDLE thandle;
	int ret;
	const char *sidlist[] = {
		"S-1-5-32-544",	/* Administrators */
		"S-1-5-32-551", /* Backup Operators */
		NULL
	};

	(void)bsdtar;/* UNUSED */
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &thandle) == 0) {
		bsdtar_warnc(bsdtar, EPERM, "Failed to check privilege");
		return (0);
	}
	ret = _is_privileged(thandle, sidlist);
	if (ret < 0) {
		bsdtar_warnc(bsdtar, errno, "Failed to check privilege");
		return (0);
	}
	return (ret);
}

/*
 * Note: We should use wide-character for findng '\' character,
 * a directory separator on Windows, because some character-set have
 * been using the '\' character for a part of its multibyte character
 * code.
 */
static size_t
dir_len_w(const char *path)
{
	wchar_t wc;
	const char *p, *rp;
	size_t al, l, size;

	al = l = -1;
	for (p = path; *p != '\0'; ++p) {
		if (*p == '\\')
			al = l = p - path;
		else if (*p == '/')
			al = p - path;
	}
	if (l == -1)
		goto alen;
	size = p - path;
	rp = p = path;
	while (*p != '\0') {
		l = mbtowc(&wc, p, size);
		if (l == -1)
			goto alen;
		if (l == 1 && (wc == L'/' || wc == L'\\'))
			rp = p;
		p += l;
		size -= l;
	}
	return (rp - path + 1);
alen:
	if (al == -1)
		return (0);
	return (al + 1);
}

/*
 * Find file names and call write_hierarchy function.
 */
void
write_hierarchy_win(struct bsdtar *bsdtar, struct archive *a,
    const char *path, void (*write_hierarchy)(struct bsdtar *bsdtar,
    struct archive *a, const char *path))
{
	DIR *dir;
	struct dirent *ent;
	const char *r;
	char *xpath;
	size_t dl;

	r = path;
	while (*r != '\0' && *r != '*' && *r != '?')
		++r;
	if (*r == '\0')
		/* There aren't meta-characters '*' and '?' in path */
		goto try_plain;
	dir = opendir_findfile(path);
	if (dir == NULL)
		goto try_plain;
	dl = dir_len_w(path);
	xpath = malloc(dl + MAX_PATH);
	if (xpath == NULL)
		goto try_plain;
	strncpy(xpath, path, dl);
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.' && ent->d_name[1] == '\0')
			continue;
		if (ent->d_name[0] == '.' && ent->d_name[1] == '.' &&
		    ent->d_name[2] == '\0')
			continue;
		strcpy(&xpath[dl], ent->d_name);
		write_hierarchy(bsdtar, a, xpath);
	}
	free(xpath);
	closedir(dir);
	return;

try_plain:
	write_hierarchy(bsdtar, a, path);
}

#endif /* LIST_H */

#endif
