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
 */

#include "archive_platform.h"

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archive.h"
#include "archive_integer.h"
#include "archive_platform_stat.h"

/* Android's LP32 stdio interfaces become 64-bit only at API 24. */
#if defined(__ANDROID__) && !defined(__LP64__)
#if __ANDROID_API__ < 24
#define USE_ANDROID_STDIO32
#else
#define USE_ANDROID_STDIO64
#endif
#endif

struct read_FILE_data {
	FILE    *f;
	size_t	 block_size;
	int64_t	 size;
	void	*buffer;
	char	 can_skip;
};

static int	FILE_close(struct archive *, void *);
static ssize_t	FILE_read(struct archive *, void *, const void **buff);
static int64_t	FILE_seek(struct archive *, void *, int64_t, int);
static int64_t	FILE_skip(struct archive *, void *, int64_t);
static int64_t	FILE_tell(struct read_FILE_data *);
static int	FILE_reposition(struct read_FILE_data *, int64_t, int);

int
archive_read_open_FILE(struct archive *a, FILE *f)
{
	la_seek_stat_t st;
	struct read_FILE_data *mine;
	size_t block_size = 128 * 1024;
	void *b;
	int r;
	int can_seek = 1;

	archive_clear_error(a);
	mine = calloc(1, sizeof(*mine));
	b = malloc(block_size);
	if (mine == NULL || b == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		free(mine);
		free(b);
		return (ARCHIVE_FATAL);
	}
	mine->block_size = block_size;
	mine->buffer = b;
	mine->f = f;
	/*
	 * If we can't fstat() the file, it may just be that it's not
	 * a file.  (On some platforms, FILE * objects can wrap I/O
	 * streams that don't support fileno()).  As a result, fileno()
	 * should be used cautiously.)
	 */
	if (la_seek_fstat(fileno(mine->f), &st) == 0) {
		if (S_ISREG(st.st_mode)) {
			archive_read_extract_set_skip_file(a, st.st_dev, st.st_ino);
			/* Enable the seek optimization only for regular files. */
			mine->can_skip = 1;
			mine->size = st.st_size;
#if defined(USE_ANDROID_STDIO32)
			/*
			 * Narrow FILE positions cannot safely represent
			 * offsets throughout a large regular file.
			 */
			if (st.st_size < 0 ||
			    st.st_size > LONG_MAX) {
				mine->can_skip = 0;
				can_seek = 0;
			}
#endif
		}
		if (S_ISFIFO(st.st_mode))
			can_seek = 0;
#ifdef S_ISSOCK
		if (S_ISSOCK(st.st_mode))
			can_seek = 0;
#endif
	}

#if defined(USE_ANDROID_STDIO32)
	{
		int fd = fileno(f);

		if (fd >= 0) {
			off64_t physical =
			    lseek64(fd, 0, SEEK_CUR);

			if (physical < 0 ||
			    physical > LONG_MAX) {
				mine->can_skip = 0;
				can_seek = 0;
			}
		}
	}
#endif
#if defined(__CYGWIN__) || defined(_WIN32)
	setmode(fileno(mine->f), O_BINARY);
#endif

	archive_read_set_read_callback(a, FILE_read);
	archive_read_set_skip_callback(a, can_seek ? FILE_skip : NULL);
	archive_read_set_seek_callback(a, can_seek ? FILE_seek : NULL);
	archive_read_set_close_callback(a, FILE_close);
	r = archive_read_set_callback_data(a, mine);
	if (r < 0) {
		free(mine->buffer);
		free(mine);
		return (r);
	}
	return (archive_read_open1(a));
}

static ssize_t
FILE_read(struct archive *a, void *client_data, const void **buff)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;
	size_t bytes_read;

	*buff = mine->buffer;
	bytes_read = fread(mine->buffer, 1, mine->block_size, mine->f);
	if (bytes_read < mine->block_size && ferror(mine->f)) {
		archive_set_error(a, errno, "Error reading file");
		return (-1);
	}
	return (bytes_read);
}

static int64_t
FILE_tell(struct read_FILE_data *mine)
{
	FILE *f = mine->f;

#if defined(USE_ANDROID_STDIO32)
	return (ftell(f));
#elif defined(USE_ANDROID_STDIO64)
	return (ftello64(f));
#elif HAVE__FSEEKI64
	return (_ftelli64(f));
#elif HAVE_FSEEKO
	return (ftello(f));
#else
	return (ftell(f));
#endif
}

static int
FILE_reposition(struct read_FILE_data *mine, int64_t offset, int whence)
{
	FILE *f = mine->f;

#if defined(USE_ANDROID_STDIO32)
	int64_t target = offset;
	la_seek_stat_t st;
	int fd;

	/*
	 * Old Bionic has a narrow FILE position cache.
	 * Never mix descriptor seeks with buffered stdio.
	 */
	if (whence == SEEK_CUR) {
		long old_offset = ftell(f);

		if (old_offset < 0)
			return (-1);

		if (archive_ckd_add_i64(
		    &target, old_offset, offset)) {
			errno = EOVERFLOW;
			return (-1);
		}

		whence = SEEK_SET;
	} else if (whence == SEEK_END) {
		fd = fileno(f);

		if (fd >= 0) {
			if (la_seek_fstat(fd, &st) != 0 ||
			    !S_ISREG(st.st_mode) ||
			    st.st_size < 0 ||
			    st.st_size > LONG_MAX ||
			    archive_ckd_add_i64(
			    &target, (int64_t)st.st_size, offset)) {
				errno = EOVERFLOW;
				return (-1);
			}

			whence = SEEK_SET;
		}
	}

	if (target < LONG_MIN ||
	    target > LONG_MAX ||
	    (whence == SEEK_SET && target < 0)) {
		errno = EOVERFLOW;
		return (-1);
	}

	return (fseek(f, (long)target, whence));
#elif defined(USE_ANDROID_STDIO64)
	return (fseeko64(f, (off64_t)offset, whence));
#elif HAVE__FSEEKI64
	return (_fseeki64(f, offset, whence));
#elif HAVE_FSEEKO
	return (fseeko(f, (off_t)offset, whence));
#else
	return (fseek(f, (long)offset, whence));
#endif
}

static int64_t
FILE_skip(struct archive *a, void *client_data, int64_t request)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;
#if defined(USE_ANDROID_STDIO32) || defined(USE_ANDROID_STDIO64) || \
    HAVE__FSEEKI64
	int64_t skip = request;
#elif HAVE_FSEEKO
	off_t skip = (off_t)request;
#else
	long skip = (long)request;
#endif
	int64_t old_offset, new_offset;
	int skip_bits = sizeof(skip) * 8 - 1;

	/*
	 * If we can't skip, return 0 as the amount we did step and
	 * the caller will work around by reading and discarding.
	 */
	if (!mine->can_skip)
		return (0);
	if (ferror(mine->f))
		return (0);
	if (request == 0)
		return (0);

	/* If request is too big for a long or an off_t, reduce it. */
	if (skip_bits < 63) {
		const int64_t max_skip =
		    (((int64_t)1 << (skip_bits - 1)) - 1) * 2 + 1;
		if (request > max_skip)
			skip = max_skip;
	}

	old_offset = FILE_tell(mine);
	if (old_offset < 0) {
		mine->can_skip = 0;
		return (0);
	}
	if (old_offset < mine->size &&
	    skip <= mine->size - old_offset) {
		if (FILE_reposition(mine, skip, SEEK_CUR) == 0) {
			new_offset = FILE_tell(mine);
			if (new_offset >= 0)
				return (new_offset - old_offset);
		}
		mine->can_skip = 0;
		if (FILE_tell(mine) != old_offset &&
		    FILE_reposition(mine, old_offset, SEEK_SET) != 0) {
			archive_set_error(a, errno, "Error skipping in FILE* pointer");
			return (ARCHIVE_FATAL);
		}
		/* Discard an error from the rejected skip, not a prior error. */
		clearerr(mine->f);
	}

	return (0);
}

/*
 * TODO: Store the offset and use it in the read callback.
 */
static int64_t
FILE_seek(struct archive *a, void *client_data, int64_t request, int whence)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;
#if defined(USE_ANDROID_STDIO32) || defined(USE_ANDROID_STDIO64) || \
    HAVE__FSEEKI64
	int64_t seek = request;
#elif HAVE_FSEEKO
	off_t seek = (off_t)request;
#else
	long seek = (long)request;
#endif
	int seek_bits = sizeof(seek) * 8 - 1;
	int64_t old_offset, new_offset;
	int had_error;

	/* Do not perform a seek which cannot be fulfilled. */
	if (seek_bits < 63) {
		const int64_t max_seek =
		    (((int64_t)1 << (seek_bits - 1)) - 1) * 2 + 1;
		const int64_t min_seek = ~max_seek;
		if (request < min_seek || request > max_seek) {
			errno = EOVERFLOW;
			archive_set_error(a, errno, "Error seeking in FILE* pointer");
			return (ARCHIVE_FAILED);
		}
	}

	old_offset = FILE_tell(mine);
	had_error = ferror(mine->f);
	if (FILE_reposition(mine, seek, whence) == 0) {
		new_offset = FILE_tell(mine);
		if (new_offset >= 0)
			return (new_offset);
	}
	archive_set_error(a, errno, "Error seeking in FILE* pointer");
	new_offset = FILE_tell(mine);
	if (old_offset >= 0 &&
	    (new_offset == old_offset ||
	    FILE_reposition(mine, old_offset, SEEK_SET) == 0) &&
	    !had_error) {
		/* Discard an error from the rejected seek, not a prior error. */
		clearerr(mine->f);
		return (ARCHIVE_FAILED);
	}
	return (ARCHIVE_FATAL);
}

static int
FILE_close(struct archive *a, void *client_data)
{
	struct read_FILE_data *mine = (struct read_FILE_data *)client_data;

	(void)a; /* UNUSED */
	free(mine->buffer);
	free(mine);
	return (ARCHIVE_OK);
}
