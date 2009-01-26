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

/* Lzma filter */
static ssize_t	lzma_filter_read(struct archive_read_filter *, const void **);
static int	lzma_filter_close(struct archive_read_filter *);
#endif

/*
 * Note that we can detect lzma archives even if we can't decompress
 * them.  (In fact, we like detecting them because we can give better
 * error messages.)  So the bid framework here gets compiled even
 * if lzmadec is unavailable.
 */
static int	lzma_bidder_bid(struct archive_read_filter_bidder *, struct archive_read_filter *);
static int	lzma_bidder_init(struct archive_read_filter *);
static int	lzma_bidder_free(struct archive_read_filter_bidder *);

int
archive_read_support_compression_lzma(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct archive_read_filter_bidder *bidder = __archive_read_get_bidder(a);

	if (bidder == NULL)
		return (ARCHIVE_FATAL);

	bidder->data = NULL;
	bidder->bid = lzma_bidder_bid;
	bidder->init = lzma_bidder_init;
	bidder->options = NULL;
	bidder->free = lzma_bidder_free;
	return (ARCHIVE_OK);
}

static int
lzma_bidder_free(struct archive_read_filter_bidder *self){
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
lzma_bidder_bid(struct archive_read_filter_bidder *self,
    struct archive_read_filter *filter)
{
	const unsigned char *buffer;
	size_t avail;
	int bits_checked;

	(void)self; /* UNUSED */

	buffer = __archive_read_filter_ahead(filter, 6, &avail);
	if (buffer == NULL)
		return (0);

	/* First byte of raw LZMA stream is always 0x5d. */
	bits_checked = 0;
	if (buffer[0] != 0x5d)
		return (0);
	bits_checked += 8;

	/* Second through fifth bytes are dictionary code, stored in
	 * little-endian order.  The two least-significant bytes are
	 * always zero. */
	if (buffer[1] != 0 || buffer[2] != 0)
		return (0);
	bits_checked += 16;

	/* ??? TODO:  Fix this. ??? */
	/* NSIS format check uses this, but I've seen tar.lzma
	 * archives where this byte is 0xff, not 0.  Can it
	 * ever be anything other than 0 or 0xff?
	 */
#if 0
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
static int
lzma_bidder_init(struct archive_read_filter *filter)
{
	(void)filter;	/* UNUSED */

	archive_set_error(&filter->archive->archive, -1,
	    "This version of libarchive was compiled without lzma support");
	return (ARCHIVE_FATAL);
}


#else

/*
 * Setup the callbacks.
 */
static int
lzma_bidder_init(struct archive_read_filter *self)
{
	static const size_t out_block_size = 64 * 1024;
	void *out_block;
	const char *buff;
	struct private_data *state;
	int ret;

	self->code = ARCHIVE_COMPRESSION_LZMA;
	self->name = "lzma";

	state = (struct private_data *)calloc(sizeof(*state), 1);
	out_block = (unsigned char *)malloc(out_block_size);
	if (state == NULL || out_block == NULL) {
		archive_set_error(&self->archive->archive, ENOMEM,
		    "Can't allocate data for lzma decompression");
		free(out_block);
		free(state);
		return (ARCHIVE_FATAL);
	}

	self->data = state;
	state->out_block_size = out_block_size;
	state->out_block = out_block;
	self->read = lzma_filter_read;
	self->skip = NULL; /* not supported */
	self->close = lzma_filter_close;

	/*
	 * Prime the lzma library with at least 18 bytes
	 * of input.  (But give it as much as is available.)
	 */
	buff = __archive_read_filter_ahead(self->upstream, 18, &ret);
	if (buff == NULL)
		return (ARCHIVE_FATAL);
	__archive_read_filter_consume(self->upstream, ret);
	/*
	 * zlib.h made this mistake and people keep copying it.  <sigh>
	 * stream.next_in should be const but isn't, hence this very
	 * ugly cast.
	 */
	state->stream.next_in = (unsigned char *)(uintptr_t)buff;
	state->stream.avail_in = ret;

	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	/* Initialize compression library. */
	ret = lzmadec_init(&(state->stream));

	if (ret == LZMADEC_OK)
		return (ARCHIVE_OK);

	/* Library setup failed: Clean up. */
	archive_set_error(&self->archive->archive, ARCHIVE_ERRNO_MISC,
	    "Internal error initializing lzma library");

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case LZMADEC_HEADER_ERROR:
		archive_set_error(&self->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid header");
		break;
	case LZMADEC_MEM_ERROR:
		archive_set_error(&self->archive->archive, ENOMEM,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	}

	free(state->out_block);
	free(state);
	self->data = NULL;
	return (ARCHIVE_FATAL);
}

/*
 * Return the next block of decompressed data.
 */
static ssize_t
lzma_filter_read(struct archive_read_filter *self, const void **p)
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
	while (state->stream.avail_out > 0 && !state->eof) {
		/* If the last upstream block is done, get another one. */
		if (state->stream.avail_in == 0) {
			read_buf = __archive_read_filter_ahead(self->upstream,
			    1, &ret);
			if (ret == 0) {
				state->eof = 1;
				break;
			}
			if (read_buf == NULL || ret < 0)
				return (ARCHIVE_FATAL);
			/* stream.next_in is really const, but lzmadec
			 * doesn't declare it so. <sigh> */
			state->stream.next_in
			    = (unsigned char *)(uintptr_t)read_buf;
			state->stream.avail_in = ret;
			__archive_read_filter_consume(self->upstream, ret);
		}

		/* Decompress as much as we can in one pass. */
		ret = lzmadec_decode(&(state->stream),
		    state->stream.avail_in == 0);
		switch (ret) {
		case LZMADEC_STREAM_END: /* Found end of stream. */
			state->eof = 1;
		case LZMADEC_OK: /* Decompressor made some progress. */
			break;
		case LZMADEC_BUF_ERROR: /* Insufficient input data? */
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "Insufficient compressed data");
			return (ARCHIVE_FATAL);
		default:
			/* Return an error. */
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "%s decompression failed",
			    self->archive->archive.compression_name);
			return (ARCHIVE_FATAL);
		}
	}

	*p = state->out_block;
	decompressed = state->stream.next_out - state->out_block;
	state->total_out += decompressed;
	return (decompressed);
}

/*
 * Clean up the decompressor.
 */
static int
lzma_filter_close(struct archive_read_filter *self)
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
	return (ret);
}

#endif /* HAVE_LZMADEC_H */
