/*-
 * Copyright (c) 2026 Tobias Stoeckmann
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "archive.h"
#include "archive_read_private.h"

struct read_archive_data {
	struct archive_read	*archive;
	int64_t			 remaining;
	ssize_t			 unconsumed;
};

static int	archive_close(struct archive *, void *);
static ssize_t	archive_read(struct archive *, void *, const void **);

int
__archive_read_open_archive(struct archive *a, struct archive *_ar,
    int64_t size)
{
	struct archive_read *ar = (struct archive_read *)_ar;
	struct read_archive_data *mine;
	int r;

	archive_clear_error(a);

	archive_check_magic(_ar, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_ANY,
	    "archive_read_open_archive");

	mine = malloc(sizeof(*mine));
	if (mine == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		free(mine);
		return (ARCHIVE_FATAL);
	}
	mine->archive = ar;
	mine->remaining = size;
	mine->unconsumed = 0;

	archive_read_set_read_callback(a, archive_read);
	archive_read_set_close_callback(a, archive_close);
	r = archive_read_set_callback_data(a, mine);
	if (r < 0) {
		free(mine);
		return (r);
	}
	return (archive_read_open1(a));
}

static ssize_t
archive_read(struct archive *a, void *data, const void **buff)
{
	struct read_archive_data *mine = (struct read_archive_data *)data;
	ssize_t bytes_read;

	(void)a; /* UNUSED */
	if (mine->unconsumed != 0) {
		__archive_read_consume(mine->archive, mine->unconsumed);
		mine->unconsumed = 0;
	}
	*buff = __archive_read_ahead(mine->archive, 1, &bytes_read);
	if (bytes_read > mine->remaining)
		bytes_read = (ssize_t)mine->remaining;
	if (bytes_read > 0) {
		mine->remaining -= bytes_read;
		mine->unconsumed = bytes_read;
	}

	return (bytes_read);
}

static int
archive_close(struct archive *a, void *data)
{
	struct read_archive_data *mine = (struct read_archive_data *)data;
	int r;

	(void)a; /* UNUSED */
	r = __archive_read_consume(mine->archive,
	    mine->unconsumed + mine->remaining);
	
	free(mine);
	return (r);
}
