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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_compression_none.c 201080 2009-12-28 02:03:54Z kientzle $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_write_private.h"


int
archive_write_set_compression_none(struct archive *a)
{
	__archive_write_filters_free(a);
	return (archive_write_add_filter_none(a));
}


static int archive_compressor_none_open(struct archive_write_filter *);
static int archive_compressor_none_write(struct archive_write_filter *,
		    const void *, size_t);
static int archive_compressor_none_close(struct archive_write_filter *);
static int archive_compressor_none_free(struct archive_write_filter *);

struct archive_none {
	char	*buffer;
	ssize_t	 buffer_size;
	char	*next;		/* Current insert location */
	ssize_t	 avail;		/* Free space left in buffer */
};

/*
 * TODO: A little refactoring will turn this into a true no-op.
 */
int
archive_write_add_filter_none(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct archive_write_filter *f = __archive_write_allocate_filter(_a);
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_compression_none");
	f->open = &archive_compressor_none_open;
	f->code = ARCHIVE_COMPRESSION_NONE;
	f->name = "none";

	return (ARCHIVE_OK);
}

/*
 * Setup callback.
 */
static int
archive_compressor_none_open(struct archive_write_filter *f)
{
	int ret;
	struct archive_none *state;

	ret = __archive_write_open_filter(f->next_filter);
	if (ret != 0)
		return (ret);

	f->bytes_per_block = archive_write_get_bytes_per_block(f->archive);
	f->bytes_in_last_block = archive_write_get_bytes_in_last_block(f->archive);

	state = (struct archive_none *)calloc(1, sizeof(*state));
	if (state == NULL) {
		archive_set_error(f->archive, ENOMEM,
		    "Can't allocate data for output buffering");
		return (ARCHIVE_FATAL);
	}

	state->buffer_size = f->bytes_per_block;
	if (state->buffer_size != 0) {
		state->buffer = (char *)malloc(state->buffer_size);
		if (state->buffer == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate output buffer");
			free(state);
			return (ARCHIVE_FATAL);
		}
	}

	state->next = state->buffer;
	state->avail = state->buffer_size;

	f->data = state;
	f->write = archive_compressor_none_write;
	f->close = archive_compressor_none_close;
	f->free = archive_compressor_none_free;
	return (ARCHIVE_OK);
}

/*
 * Write data to the stream.
 */
static int
archive_compressor_none_write(struct archive_write_filter *f,
    const void *vbuff, size_t length)
{
	struct archive_none *state = (struct archive_none *)f->data;
	const char *buff;
	ssize_t remaining, to_copy;
	int ret;

	/*
	 * If there is no buffer for blocking, just pass the data
	 * straight through to the client write callback.  In
	 * particular, this supports "no write delay" operation for
	 * special applications.  Just set the block size to zero.
	 */
	if (state->buffer_size == 0)
		return (__archive_write_filter(f->next_filter,
			vbuff, length));

	buff = (const char *)vbuff;
	remaining = length;

	/* If the copy buffer isn't empty, try to fill it. */
	if (state->avail < state->buffer_size) {
		/* If buffer is not empty... */
		/* ... copy data into buffer ... */
		to_copy = (remaining > state->avail) ?
		    state->avail : remaining;
		memcpy(state->next, buff, to_copy);
		state->next += to_copy;
		state->avail -= to_copy;
		buff += to_copy;
		remaining -= to_copy;
		/* ... if it's full, write it out. */
		if (state->avail == 0) {
			ret = __archive_write_filter(f->next_filter,
			    state->buffer, state->buffer_size);
			if (ret != ARCHIVE_OK)
				return (ARCHIVE_FATAL);
			state->next = state->buffer;
			state->avail = state->buffer_size;
		}
	}

	/* Write out full blocks directly to client. */
	while (remaining > state->buffer_size) {
		ret = __archive_write_filter(f->next_filter,
		    buff, state->buffer_size);
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		buff += state->buffer_size;
		remaining -= state->buffer_size;
	}

	if (remaining > 0) {
		/* Copy last bit into copy buffer. */
		memcpy(state->next, buff, remaining);
		state->next += remaining;
		state->avail -= remaining;
	}

	return (ARCHIVE_OK);
}


/*
 * Finish the compression.
 */
static int
archive_compressor_none_close(struct archive_write_filter *f)
{
	struct archive_none *state = (struct archive_none *)f->data;
	ssize_t block_length;
	ssize_t target_block_length;
	int ret;

	ret = ARCHIVE_OK;

	/* If there's pending data, pad and write the last block */
	if (state->next != state->buffer) {
		block_length = state->buffer_size - state->avail;

		/* Tricky calculation to determine size of last block */
		if (f->bytes_in_last_block <= 0)
			/* Default or Zero: pad to full block */
			target_block_length = f->bytes_per_block;
		else
			/* Round to next multiple of bytes_in_last_block. */
			target_block_length = f->bytes_in_last_block *
			    ( (block_length + f->bytes_in_last_block - 1) /
				f->bytes_in_last_block);
		if (target_block_length > f->bytes_per_block)
			target_block_length = f->bytes_per_block;
		if (block_length < target_block_length) {
			memset(state->next, 0,
			    target_block_length - block_length);
			block_length = target_block_length;
		}
		ret = __archive_write_filter(f->next_filter,
		    state->buffer, block_length);
	}
	if (state->buffer)
		free(state->buffer);
	free(state);
	f->data = NULL;

	return (ret);
}

static int
archive_compressor_none_free(struct archive_write_filter *f)
{
	(void)f; /* UNUSED */
	return (ARCHIVE_OK);
}
