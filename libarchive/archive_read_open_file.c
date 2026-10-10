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
#include "archive_platform_stat.h"

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
static int64_t	FILE_tell(FILE *);
static int	FILE_reposition(FILE *, int64_t, int);

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
		}
		if (S_ISFIFO(st.st_mode))
			can_seek = 0;
#ifdef S_ISSOCK
		if (S_ISSOCK(st.st_mode))
			can_seek = 0;
#endif
	}

#if defined(__CYGWIN__) || defined(_WIN32)
	setmode(fileno(mine->f), O_BINARY);
#endif

	archive_read_set_read_callback(a, FILE_read);
	archive_read_set_skip_callback(a, FILE_skip);
	if (can_seek)
		archive_read_set_seek_callback(a, FILE_seek);
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
FILE_tell(FILE *f)
{
#if HAVE__FSEEKI64
	return (_ftelli64(f));
#elif HAVE_FSEEKO
	return (ftello(f));
#else
	return (ftell(f));
#endif
}

static int
FILE_reposition(FILE *f, int64_t offset, int whence)
{
#if HAVE__FSEEKI64
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
#if HAVE__FSEEKI64
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

	old_offset = FILE_tell(mine->f);
	if (old_offset < 0) {
		mine->can_skip = 0;
		return (0);
	}
	if (old_offset < mine->size &&
	    skip <= mine->size - old_offset) {
		if (FILE_reposition(mine->f, skip, SEEK_CUR) == 0) {
			new_offset = FILE_tell(mine->f);
			if (new_offset >= 0)
				return (new_offset - old_offset);
		}
		if (FILE_reposition(mine->f, old_offset, SEEK_SET) != 0) {
			archive_set_error(a, errno, "Error skipping in FILE* pointer");
			return (ARCHIVE_FATAL);
		}
		mine->can_skip = 0;
		/* A successful reposition need not clear a stream error. */
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
#if HAVE__FSEEKI64
	int64_t seek = request;
#elif HAVE_FSEEKO
	off_t seek = (off_t)request;
#else
	long seek = (long)request;
#endif
	int seek_bits = sizeof(seek) * 8 - 1;
	int64_t old_offset, new_offset;
	int had_error, saved_errno;

	/* Do not perform a seek which cannot be fulfilled. */
	if (seek_bits < 63) {
		const int64_t max_seek =
		    (((int64_t)1 << (seek_bits - 1)) - 1) * 2 + 1;
		const int64_t min_seek = ~max_seek;
		if (request < min_seek || request > max_seek) {
			errno = EOVERFLOW;
			archive_set_error(a, errno, "Error seeking in FILE* pointer");
			return (ARCHIVE_FATAL);
		}
	}

	old_offset = FILE_tell(mine->f);
	had_error = ferror(mine->f);
	if (FILE_reposition(mine->f, seek, whence) == 0) {
		new_offset = FILE_tell(mine->f);
		if (new_offset >= 0)
			return (new_offset);
	}
	saved_errno = errno;
	if (old_offset >= 0 &&
	    FILE_reposition(mine->f, old_offset, SEEK_SET) == 0 &&
	    !had_error) {
		/* Discard an error from the rejected seek, not a prior error. */
		clearerr(mine->f);
		archive_set_error(a, saved_errno, "Error seeking in FILE* pointer");
		return (ARCHIVE_FAILED);
	}
	archive_set_error(a, saved_errno, "Error seeking in FILE* pointer");
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
