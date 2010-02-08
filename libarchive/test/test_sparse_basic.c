/*-
 * Copyright (c) 2010 Michihiro NAKAJIMA
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
 */
#include "test.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/*
 * NOTE: On FreeBSD and Solaris, this test needs ZFS.
 * You may should perfom this test as
 * 'TMPDIR=<a directory on the ZFS> libarchive_test'.
 */

struct sparse {
	enum { DATA, HOLE, END } type;
	size_t	size;
};

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <winioctl.h>
/*
 * Create a sparse file on Windows.
 */

#if !defined(PATH_MAX)
#define	PATH_MAX	MAX_PATH
#endif
#if !defined(__BORLANDC__)
#define getcwd _getcwd
#endif

static int
is_sparse_supported(const char *path)
{
	char root[MAX_PATH+1];
	char vol[MAX_PATH+1];
	char sys[MAX_PATH+1];
	DWORD flags;
	BOOL r;

	strncpy(root, path, sizeof(root)-1);
	if (((root[0] >= 'c' && root[0] <= 'z') ||
	    (root[0] >= 'C' && root[0] <= 'Z')) &&
		root[1] == ':' &&
	    (root[2] == '\\' || root[2] == '/'))
		root[3] = '\0';
	else
		return (0);
	assertEqualInt((r = GetVolumeInformation(root, vol,
	    sizeof(vol), NULL, NULL, &flags, sys, sizeof(sys))), 1);
	return (r != 0 && (flags & FILE_SUPPORTS_SPARSE_FILES) != 0);
}

static void
create_sparse_file(const char *path, const struct sparse *s)
{
	char buff[1024];
	HANDLE handle;
	DWORD dmy;

	memset(buff, ' ', sizeof(buff));

	handle = CreateFileA(path, GENERIC_WRITE, 0,
	    NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
	    NULL);
	assert(handle != INVALID_HANDLE_VALUE);
	assert(DeviceIoControl(handle, FSCTL_SET_SPARSE, NULL, 0,
	    NULL, 0, &dmy, NULL) != 0);
	while (s->type != END) {
		if (s->type == HOLE) {
			LARGE_INTEGER distance;

			distance.QuadPart = s->size;
			assert(SetFilePointerEx(handle, distance,
			    NULL, FILE_CURRENT) != 0);
		} else {
			DWORD w, wr, size;

			size = s->size;
			while (size) {
				if (size > sizeof(buff))
					w = sizeof(buff);
				else
					w = size;
				assert(WriteFile(handle, buff, w, &wr, NULL) != 0);
				size -= wr;
			}
		}
		s++;
	}
	assertEqualInt(CloseHandle(handle), 1);
}

#else

#if defined(_PC_MIN_HOLE_SIZE)

/*
 * FreeBSD and Solaris can detect 'hole' of a sparse file
 * through lseek(HOLE) on ZFS. (UFS does not support yet)
 */

static int
is_sparse_supported(const char *path)
{
	return (pathconf(path, _PC_MIN_HOLE_SIZE) > 0);
}

#elif defined(__linux__)

/*
 * FIEMAP, which can detect 'hole' of a sparse file, has
 * been supported from 2.6.28
 */

static int
is_sparse_supported(const char *path)
{
	struct utsname ut;
	char *p, *e;
	long d;

	memset(&ut, 0, sizeof(ut));
	assertEqualInt(uname(&ut), 0);
	p = ut.release;
	d = strtol(p, &e, 10);
	if (d < 2 || *e != '.')
		return (0);
	p = e + 1;
	d = strtol(p, &e, 10);
	if (d < 6 || *e != '.')
		return (0);
	p = e + 1;
	d = strtol(p, NULL, 10);
	return (d >= 28);
}

#else

/*
 * Other system may do not have the API such as lseek(HOLE),
 * which detect 'hole' of a sparse file.
 */

static int
is_sparse_supported(const char *path)
{
	return (0);
}

#endif

/*
 * Create a sparse file on POSIX like system.
 */

static void
create_sparse_file(const char *path, const struct sparse *s)
{
	char buff[1024];
	int fd;

	memset(buff, ' ', sizeof(buff));
	assert((fd = open(path, O_CREAT | O_WRONLY, 0600)) != -1);
	while (s->type != END) {
		if (s->type == HOLE) {
			assert(lseek(fd, s->size, SEEK_CUR) != (off_t)-1);
		} else {
			size_t w, size;

			size = s->size;
			while (size) {
				if (size > sizeof(buff))
					w = sizeof(buff);
				else
					w = size;
				assert(write(fd, buff, w) != (ssize_t)-1);
				size -= w;
			}
		}
		s++;
	}
	close(fd);
}

#endif

static void
verify_sparse_file(struct archive *a, const char *path,
    const struct sparse *sparse, int blocks)
{
	struct stat stb, *st;
	struct archive_entry *ae;

	st = &stb;
	create_sparse_file(path, sparse);
#if defined(_WIN32) && !defined(__CYGWIN__)
	st = NULL;
#else
	assertEqualInt(stat(path, &stb), 0);
#endif
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, path);
	archive_entry_copy_sourcepath(ae, path);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, st));
	/* Verify the number of holes only, not its offset nor its
	 * length because those alignments are deeply dependence on
	 * its filesystem. */ 
	assertEqualInt(blocks, archive_entry_sparse_count(ae));
	archive_entry_free(ae);
}

DEFINE_TEST(test_sparse_basic)
{
	char cwd[PATH_MAX+1];
	char path[PATH_MAX+1];
	char *p;
	struct archive *a;
	/*
	 * The alignment of the hole of sparse files deeply depends
	 * on filesystem. In my experience, sparse_file2 test with
	 * 204800 bytes hole size did not pass on ZFS and the result
	 * of that test seemed the size was too small, thus you should
	 * keep a hole size more than 409600 bytes to pass this test
	 * on all platform.
	 */
	const struct sparse sparse_file0[] = {
		{ DATA,	 1024 }, { HOLE,   2048000 },
		{ DATA,	 2048 }, { HOLE,   2048000 },
		{ DATA,	 4096 }, { HOLE,  20480000 },
		{ DATA,	 8192 }, { HOLE, 204800000 },
		{ DATA,     1 }, { END,	0 }
	};
	const struct sparse sparse_file1[] = {
		{ HOLE,	409600 }, { DATA, 1 },
		{ HOLE,	409600 }, { DATA, 1 },
		{ HOLE,	409600 }, { END,  0 }
	};
	const struct sparse sparse_file2[] = {
		{ HOLE,	409600 * 1 }, { DATA, 1024 },
		{ HOLE,	409600 * 2 }, { DATA, 1024 },
		{ HOLE,	409600 * 3 }, { DATA, 1024 },
		{ HOLE,	409600 * 4 }, { DATA, 1024 },
		{ HOLE,	409600 * 5 }, { DATA, 1024 },
		{ HOLE,	409600 * 6 }, { DATA, 1024 },
		{ HOLE,	409600 * 7 }, { DATA, 1024 },
		{ HOLE,	409600 * 8 }, { DATA, 1024 },
		{ HOLE,	409600 * 9 }, { DATA, 1024 },
		{ HOLE,	409600 * 10}, { DATA, 1024 },/* 10 */
		{ HOLE,	409600 * 1 }, { DATA, 1024 * 1 },
		{ HOLE,	409600 * 2 }, { DATA, 1024 * 2 },
		{ HOLE,	409600 * 3 }, { DATA, 1024 * 3 },
		{ HOLE,	409600 * 4 }, { DATA, 1024 * 4 },
		{ HOLE,	409600 * 5 }, { DATA, 1024 * 5 },
		{ HOLE,	409600 * 6 }, { DATA, 1024 * 6 },
		{ HOLE,	409600 * 7 }, { DATA, 1024 * 7 },
		{ HOLE,	409600 * 8 }, { DATA, 1024 * 8 },
		{ HOLE,	409600 * 9 }, { DATA, 1024 * 9 },
		{ HOLE,	409600 * 10}, { DATA, 1024 * 10},/* 20 */
		{ END,	0 }
	};
	const struct sparse sparse_file3[] = {
 		/* This hole size is too small to create a sparse
		 * files for almost filesystem. */
		{ HOLE,	 1024 }, { DATA, 10240 },
		{ END,	0 }
	};

	/* Make a filename template. */
	if (!assert(getcwd(cwd, sizeof(cwd)-1) != NULL))
		return;
	while (cwd[strlen(cwd) - 1] == '\n')
		cwd[strlen(cwd) - 1] = '\0';
	strncpy(path, cwd,  sizeof(path)-1);
	if (!assert(strlen(path) + 7 <= sizeof(path)))
		return;/* a filename is too long */
	if (!is_sparse_supported(path)) {
		skipping("This filesystem or platform do not support "
		    "the reporting of the holes of a sparse file through "
		    "API such as lseek(HOLE)");
		return;
	}
	p = path + strlen(path);

	assert((a = archive_read_disk_new()) != NULL);

	strcpy(p, "/file0");
	verify_sparse_file(a, path, sparse_file0, 5);

	strcpy(p, "/file1");
	verify_sparse_file(a, path, sparse_file1, 2);

	strcpy(p, "/file2");
	verify_sparse_file(a, path, sparse_file2, 20);

	strcpy(p, "/file3");
	verify_sparse_file(a, path, sparse_file3, 0);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
