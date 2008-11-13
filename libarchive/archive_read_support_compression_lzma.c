/*-
 * Copyright (c) 2003-2008 Tim Kientzle and Miklos Vajna
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

__FBSDID("$FreeBSD$");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LZMADEC_H
#include <lzmadec.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

#if HAVE_LZMADEC_H
struct private_data {
	lzmadec_stream	 stream;
	unsigned char	*out_block;
	size_t		 out_block_size;
	int64_t		 total_out;
	char		 eof; /* True = found end of compressed data. */
};

/* Lzma source */
static ssize_t	lzma_source_read(struct archive_read_source *, const void **);
static int	lzma_source_close(struct archive_read_source *);
#endif

/*
 * Note that we can detect lzma archives even if we can't decompress
 * them.  (In fact, we like detecting them because we can give better
 * error messages.)  So the bid framework here gets compiled even
 * if lzmadec is unavailable.
 */
static int	lzma_reader_bid(struct archive_reader *, const void *, size_t);
static struct archive_read_source *lzma_reader_init(struct archive_read *,
    struct archive_reader *, struct archive_read_source *,
    const void *, size_t);
static int	lzma_reader_free(struct archive_reader *);

int
archive_read_support_compression_lzma(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct archive_reader *reader = __archive_read_get_reader(a);

	if (reader == NULL)
		return (ARCHIVE_FATAL);

	reader->data = NULL;
	reader->bid = lzma_reader_bid;
	reader->init = lzma_reader_init;
	reader->free = lzma_reader_free;
	return (ARCHIVE_OK);
}

static int
lzma_reader_free(struct archive_reader *self){
	(void)self; /* UNUSED */
	return (ARCHIVE_OK);
}

/*
 * Test whether we can handle this data.
 *
 * This logic returns zero if any part of the signature fails.  It
 * also tries to Do The Right Thing if a very short buffer prevents us
 * from verifying as much as we would like.
 *
 * <sigh> LZMA has a rather poor file signature.  Zeros do not
 * make good signature bytes as a rule, and the only non-zero byte
 * here is an ASCII character.  For example, an uncompressed tar
 * archive whose first file is ']' would satisfy this check.  It may
 * be necessary to exclude LZMA from compression_all() because of
 * this.  Clients of libarchive would then have to explicitly enable
 * LZMA checking instead of (or in addition to) compression_all() when
 * they have other evidence (file name, command-line option) to go on.
 */
static int
lzma_reader_bid(struct archive_reader *self, const void *buff, size_t len)
{
	const unsigned char *buffer;
	int bits_checked;

	(void)self; /* UNUSED */

	buffer = (const unsigned char *)buff;


	/* First byte of raw LZMA stream is always 0x5d. */
	if (len < 1)
		return (0);
	bits_checked = 0;
	if (buffer[0] != 0x5d)
		return (0);
	bits_checked += 8;

	/* Second through fifth bytes are dictionary code, stored in
	 * little-endian order.  The two least-significant bytes are
	 * always zero. */
	if (len < 2)
		return (bits_checked);
	if (buffer[1] != 0)
		return (0);
	bits_checked += 8;

	if (len < 3)
		return (bits_checked);
	if (buffer[2] != 0)
		return (0);
	bits_checked += 8;

	/* ??? TODO:  Explain this.  ??? */
	/* NSIS format check uses this, but I've seen tar.lzma
	 * archives where this byte is 0xff, not 0. */
#if 0
	if (len < 6)
		return (bits_checked);
	if (buffer[5] != 0)
		return (0);
	bits_checked += 8;
#endif

	/* TODO: The above test is still very weak.  It would be
	 * good to do better. */

	return (bits_checked);
}

#ifndef HAVE_LZMADEC_H

/*
 * If we don't have the library on this system, we can't actually do the
 * decompression.  We can, however, still detect compressed archives
 * and emit a useful message.
 */
static struct archive_read_source *
lzma_reader_init(struct archive_read *a, struct archive_reader *reader,
    struct archive_read_source *upstream, const void *buff, size_t n)
{
	(void)a;	/* UNUSED */
	(void)reader;	/* UNUSED */
	(void)upstream; /* UNUSED */
	(void)buff;	/* UNUSED */
	(void)n;	/* UNUSED */

	archive_set_error(&a->archive, -1,
	    "This version of libarchive was compiled without lzma support");
	return (NULL);
}


#else

/*
 * Setup the callbacks.
 */
static struct archive_read_source *
lzma_reader_init(struct archive_read *a, struct archive_reader *reader,
    struct archive_read_source *upstream, const void *buff, size_t n)
{
	static const size_t out_block_size = 64 * 1024;
	void *out_block;
	struct archive_read_source *self;
	struct private_data *state;
	int ret;

	(void)reader; /* UNUSED */

	a->archive.compression_code = ARCHIVE_COMPRESSION_LZMA;
	a->archive.compression_name = "lzma";

	self = calloc(sizeof(*self), 1);
	state = (struct private_data *)calloc(sizeof(*state), 1);
	out_block = (unsigned char *)malloc(out_block_size);
	if (self == NULL || state == NULL || out_block == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for %s decompression",
		    a->archive.compression_name);
		free(out_block);
		free(state);
		free(self);
		return (NULL);
	}


	self->archive = a;
	self->data = state;
	state->out_block_size = out_block_size;
	state->out_block = out_block;
	self->upstream = upstream;
	self->read = lzma_source_read;
	self->skip = NULL; /* not supported */
	self->close = lzma_source_close;

	/*
	 * A bug in lzmadec.h: stream.next_in should be marked 'const'
	 * but isn't (the library never alters data through the
	 * next_in pointer, only reads it).  The result: this ugly
	 * cast to remove 'const'.
	 */
	state->stream.next_in = (char *)(uintptr_t)(const void *)buff;
	state->stream.avail_in = n;

	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	/* Initialize compression library. */
	ret = lzmadec_init(&(state->stream));

	if (ret == LZMADEC_OK)
		return (self);

	/* Library setup failed: Clean up. */
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "Internal error initializing %s library",
	    a->archive.compression_name);

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case LZMADEC_HEADER_ERROR:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid header");
		break;
	case LZMADEC_MEM_ERROR:
		archive_set_error(&a->archive, ENOMEM,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	}

	free(state->out_block);
	free(state);
	free(self);
	return (NULL);
}

/*
 * Return the next block of decompressed data.
 */
static ssize_t
lzma_source_read(struct archive_read_source *self, const void **p)
{
	struct private_data *state;
	size_t read_avail, decompressed;
	const void *read_buf;
	int ret;

	state = (struct private_data *)self->data;
	read_avail = 0;

	/* Empty our output buffer. */
	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	/* Try to fill the output buffer. */
	for (;;) {
		/* If the last upstream block is done, get another one. */
		if (state->stream.avail_in == 0) {
			ret = (self->upstream->read)(self->upstream,
			    &read_buf);
			/* stream.next_in is really const, but lzmadec
			 * doesn't declare it so. <sigh> */
			state->stream.next_in
			    = (unsigned char *)(uintptr_t)read_buf;
			if (ret < 0)
				return (ARCHIVE_FATAL);
			/* There is no more data, return whatever we have. */
			if (ret == 0) {
				*p = state->out_block;
				decompressed = state->stream.next_out
				    - state->out_block;
				state->total_out += decompressed;
				return (decompressed);
			}
			state->stream.avail_in = ret;
		}

		/* Decompress as much as we can in one pass. */
		ret = lzmadec_decode(&(state->stream),
		    state->stream.avail_in == 0);
		switch (ret) {
		case LZMADEC_STREAM_END: /* Found end of stream. */
			/* TODO: Peek ahead to see if there's another
			 * stream so we can mimic the behavior of gunzip
			 * on concatenated streams. */
			state->eof = 1;
		case LZMADEC_OK: /* Decompressor made some progress. */
			/* If we filled our buffer, update stats and return. */
			if (state->eof || state->stream.avail_out == 0) {
				*p = state->out_block;
				decompressed = state->stream.next_out
				    - state->out_block;
				state->total_out += decompressed;
				return (decompressed);
			}
			break;
		default:
			/* Return an error. */
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "%s decompression failed",
			    self->archive->archive.compression_name);
			return (ARCHIVE_FATAL);
		}
	}
}

/*
 * Clean up the decompressor.
 */
static int
lzma_source_close(struct archive_read_source *self)
{
	struct private_data *state;
	int ret;

	state = (struct private_data *)self->data;
	ret = ARCHIVE_OK;
	switch (lzmadec_end(&(state->stream))) {
	case LZMADEC_OK:
		break;
	default:
		archive_set_error(&(self->archive->archive),
		    ARCHIVE_ERRNO_MISC,
		    "Failed to clean up %s compressor",
		    self->archive->archive.compression_name);
		ret = ARCHIVE_FATAL;
	}

	free(state->out_block);
	free(state);
	free(self);
	return (ret);
}

#endif /* HAVE_LZMADEC_H */
