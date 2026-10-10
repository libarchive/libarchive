/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Tobias Stoeckmann
 * All rights reserved.
 */

/* !!ONLY FOR USE INTERNALLY TO LIBARCHIVE!! */

#ifndef ARCHIVE_PLATFORM_STAT_H_INCLUDED
#define ARCHIVE_PLATFORM_STAT_H_INCLUDED

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
/* We use _lseeki64() on Windows. */
typedef int64_t la_seek_t;

struct la_seek_stat {
	int64_t		st_mtime;
	ino_t		st_ino;
	unsigned short	st_mode;
	uint32_t	st_nlink;
	gid_t		st_gid;
	la_seek_t	st_size;
	uid_t		st_uid;
	dev_t		st_dev;
	dev_t		st_rdev;
};
typedef struct la_seek_stat la_seek_stat_t;

#define la_seek_fstat(fd, st)	__la_seek_fstat((fd), (st))
#define la_seek_stat(fd, st)	__la_seek_stat((fd), (st))

#else
typedef off_t la_seek_t;
typedef struct stat la_seek_stat_t;

#define la_seek_fstat(fd, st)	fstat((fd), (st))
#define la_seek_stat(fd, st)	stat((fd), (st))
#endif

/*
 * Tells if seeking (and skipping, which relies on it) should be used with a
 * file. A pipe or a socket can never seek. The other types of files may or may
 * not be able to seek, so this only rules out the files that are known not to
 * seek: if it returns zero, the file can't seek, but if it returns a non-zero
 * value, it may still not be able to.
 */
static inline int
la_use_seek(const la_seek_stat_t *st)
{
#ifdef S_ISFIFO
	if (S_ISFIFO(st->st_mode))
		return (0);
#endif
#ifdef S_ISSOCK
	if (S_ISSOCK(st->st_mode))
		return (0);
#endif
	(void)st; /* UNUSED */
	return (1);
}

#endif	/* !ARCHIVE_PLATFORM_STAT_H_INCLUDED */
