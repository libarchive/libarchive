/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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
#ifdef HAVE_LZMA_H
#include <lzma.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

#if HAVE_LZMA_H
struct private_data {
	lzma_stream	 stream;
	unsigned char	*out_block;
	size_t		 out_block_size;
	int64_t		 total_out;
	char		 eof; /* True = found end of compressed data. */
};

/* Lzma filter */
static ssize_t	xz_filter_read(struct archive_read_filter *, const void **);
static int	xz_filter_close(struct archive_read_filter *);
#endif

/*
 * Note that we can detect xz archives even if we can't decompress
 * them.  (In fact, we like detecting them because we can give better
 * error messages.)  So the bid framework here gets compiled even
 * if lzma is unavailable.
 */
static int	xz_lzma_bidder_init(struct archive_read_filter *, int code);
static int	lzma_bidder_bid(struct archive_read_filter_bidder *,
		    struct archive_read_filter *);
static int	lzma_bidder_init(struct archive_read_filter *);
static int	xz_bidder_bid(struct archive_read_filter_bidder *,
		    struct archive_read_filter *);
static int	xz_bidder_init(struct archive_read_filter *);
static int	xz_bidder_free(struct archive_read_filter_bidder *);

int
archive_read_support_compression_xz(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct archive_read_filter_bidder *bidder = __archive_read_get_bidder(a);

	if (bidder == NULL)
		return (ARCHIVE_FATAL);

	bidder->data = NULL;
	bidder->bid = xz_bidder_bid;
	bidder->init = xz_bidder_init;
	bidder->options = NULL;
	bidder->free = xz_bidder_free;
	return (ARCHIVE_OK);
}

#ifdef HAVE_LZMA_H
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
	bidder->free = xz_bidder_free;
	return (ARCHIVE_OK);
}
#endif

static int
xz_bidder_free(struct archive_read_filter_bidder *self){
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
 */
static int
xz_bidder_bid(struct archive_read_filter_bidder *self,
    struct archive_read_filter *filter)
{
	const unsigned char *buffer;
	ssize_t avail;
	int bits_checked;

	(void)self; /* UNUSED */

	buffer = __archive_read_filter_ahead(filter, 6, &avail);
	if (buffer == NULL)
		return (0);

	/*
	 * Verify Header Magic Bytes : FD 37 7A 58 5A 00
	 */
	bits_checked = 0;
	if (buffer[0] != 0xFD)
		return (0);
	bits_checked += 8;
	if (buffer[1] != 0x37)
		return (0);
	bits_checked += 8;
	if (buffer[2] != 0x7A)
		return (0);
	bits_checked += 8;
	if (buffer[3] != 0x58)
		return (0);
	bits_checked += 8;
	if (buffer[4] != 0x5A)
		return (0);
	bits_checked += 8;
	if (buffer[5] != 0x00)
		return (0);
	bits_checked += 8;

	return (bits_checked);
}

#ifdef HAVE_LZMA_H
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
#endif

#ifndef HAVE_LZMA_H

/*
 * If we don't have the library on this system, we can't actually do the
 * decompression.  We can, however, still detect compressed archives
 * and emit a useful message.
 */
static int
xz_bidder_init(struct archive_read_filter *filter)
{
	(void)filter;	/* UNUSED */

	archive_set_error(&filter->archive->archive, -1,
	    "This version of libarchive was compiled without xz support");
	return (ARCHIVE_FATAL);
}


#else /* HAVE_LZMA_H */

static int
xz_bidder_init(struct archive_read_filter *self)
{
	return (xz_lzma_bidder_init(self, ARCHIVE_COMPRESSION_XZ));
}

static int
lzma_bidder_init(struct archive_read_filter *self)
{
	return (xz_lzma_bidder_init(self, ARCHIVE_COMPRESSION_LZMA));
}

/*
 * Setup the callbacks.
 */
static int
xz_lzma_bidder_init(struct archive_read_filter *self, int code)
{
	static const size_t out_block_size = 64 * 1024;
	void *out_block;
	const char *buff;
	struct private_data *state;
	int ret;

	self->code = code;
	self->name = (code == ARCHIVE_COMPRESSION_XZ)?"xz": "lzma";

	state = (struct private_data *)calloc(sizeof(*state), 1);
	out_block = (unsigned char *)malloc(out_block_size);
	if (state == NULL || out_block == NULL) {
		archive_set_error(&self->archive->archive, ENOMEM,
		    "Can't allocate data for xz decompression");
		free(out_block);
		free(state);
		return (ARCHIVE_FATAL);
	}

	self->data = state;
	state->out_block_size = out_block_size;
	state->out_block = out_block;
	self->read = xz_filter_read;
	self->skip = NULL; /* not supported */
	self->close = xz_filter_close;

	state->stream.next_in = buff;
	state->stream.avail_in = 0;

	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	/* Initialize compression library.
	 * TODO: I don't know what value is best for memlimit.
	 *       maybe, it needs to check memory size which 
	 *       running system has.
	 */
	if (code == ARCHIVE_COMPRESSION_XZ)
		ret = lzma_stream_decoder(&(state->stream),
		    (1U << 23) + (1U << 21),/* memlimit */
		    LZMA_CONCATENATED);
	else
		ret = lzma_alone_decoder(&(state->stream),
		    (1U << 23) + (1U << 21));/* memlimit */

	if (ret == LZMA_OK)
		return (ARCHIVE_OK);

	/* Library setup failed: Clean up. */
	archive_set_error(&self->archive->archive, ARCHIVE_ERRNO_MISC,
	    "Internal error initializing lzma library");

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case LZMA_MEM_ERROR:
		archive_set_error(&self->archive->archive, ENOMEM,
		    "Internal error initializing compression library: "
		    "Cannot allocate memory");
		break;
	case LZMA_OPTIONS_ERROR:
		archive_set_error(&self->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "Invalid or unsupported options");
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
xz_filter_read(struct archive_read_filter *self, const void **p)
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
			/* stream.next_in is really const, but lzma
			 * doesn't declare it so. <sigh> */
			state->stream.next_in
			    = (unsigned char *)(uintptr_t)read_buf;
			state->stream.avail_in = ret;
			__archive_read_filter_consume(self->upstream, ret);
		}

		/* Decompress as much as we can in one pass. */
		ret = lzma_code(&(state->stream),
		    (state->stream.avail_in == 0)? LZMA_FINISH: LZMA_RUN);
		switch (ret) {
		case LZMA_STREAM_END: /* Found end of stream. */
			state->eof = 1;
		case LZMA_OK: /* Decompressor made some progress. */
			break;
		case LZMA_MEM_ERROR:
			archive_set_error(&self->archive->archive, ENOMEM,
			    "Internal error decompressing compression library: "
			    "Cannot allocate memory");
			return (ARCHIVE_FATAL);
		case LZMA_MEMLIMIT_ERROR:
			archive_set_error(&self->archive->archive, ENOMEM,
			    "Internal error decompressing compression library: "
			    "Memory usage limit was reached");
			return (ARCHIVE_FATAL);
		case LZMA_FORMAT_ERROR:
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "Internal error decompressing compression library: "
			    "File format not recognized");
			return (ARCHIVE_FATAL);
		case LZMA_OPTIONS_ERROR:
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "Internal error decompressing compression library: "
			    "Invalid or unsupported options");
			return (ARCHIVE_FATAL);
		case LZMA_DATA_ERROR:
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "Internal error decompressing compression library: "
			    "Data is corrupt");
			return (ARCHIVE_FATAL);
		case LZMA_BUF_ERROR:
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "Internal error decompressing compression library: "
			    "No progress is possible");
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
xz_filter_close(struct archive_read_filter *self)
{
	struct private_data *state;

	state = (struct private_data *)self->data;
	lzma_end(&(state->stream));
	free(state->out_block);
	free(state);
	return (ARCHIVE_OK);
}

#endif /* HAVE_LZMA_H */
