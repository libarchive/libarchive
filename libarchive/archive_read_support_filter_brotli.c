/*-
 * Copyright (c) 2026 François Degros
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
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_BROTLI_DECODE_H
#include <brotli/decode.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

/*
 * A Brotli stream has no signature: its first bytes depend on the window
 * size and on the first meta-block. So this filter can't be chosen by
 * looking at the data. Its bidder never bids, and the filter is only used if
 * the application asks for it, with archive_read_append_filter().
 */
static int	brotli_bidder_bid(struct archive_read_filter_bidder *,
		    struct archive_read_filter *);
static int	brotli_bidder_init(struct archive_read_filter *);

static const struct archive_read_filter_bidder_vtable
brotli_bidder_vtable = {
	.bid = brotli_bidder_bid,
	.init = brotli_bidder_init,
};

int
archive_read_support_filter_brotli(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;

	if (__archive_read_register_bidder(a, NULL,
				&brotli_bidder_vtable) != ARCHIVE_OK)
		return (ARCHIVE_FATAL);

#if HAVE_BROTLI_DECODE_H && HAVE_LIBBROTLIDEC
	return (ARCHIVE_OK);
#else
	archive_set_error(_a, ARCHIVE_ERRNO_MISC,
	    "Using external brotli program for brotli decompression");
	return (ARCHIVE_WARN);
#endif
}

/*
 * Brotli data can't be recognized. See above.
 */
static int
brotli_bidder_bid(struct archive_read_filter_bidder *b,
    struct archive_read_filter *f)
{
	(void) b; /* UNUSED */
	(void) f; /* UNUSED */

	return (0);
}

#if !(HAVE_BROTLI_DECODE_H && HAVE_LIBBROTLIDEC)

/*
 * If we don't have the library on this system, we can't do the
 * decompression directly.  We can, however, try to run "brotli -d -c"
 * in case that's available.
 */
static int
brotli_bidder_init(struct archive_read_filter *f)
{
	int r;

	r = __archive_read_program(f, "brotli -d -c");
	/* Note: We set the format here even if __archive_read_program()
	 * above fails.  We do, after all, know what the format is
	 * even if we weren't able to read it. */
	f->code = ARCHIVE_FILTER_BROTLI;
	f->name = "brotli";
	return (r);
}

#else

/* Size of the buffer that holds the decompressed data. */
#define BROTLI_OUT_BLOCK_SIZE	(64 * 1024)

struct brotli {
	BrotliDecoderState	*state;
	unsigned char		*out_block;
	char			 finished; /* True = decoded the whole stream. */
	char			 eof; /* True = no more data to return. */
};

static ssize_t	brotli_filter_read(struct archive_read_filter *, const void **);
static int	brotli_filter_close(struct archive_read_filter *);

static const struct archive_read_filter_vtable
brotli_reader_vtable = {
	.read = brotli_filter_read,
	.close = brotli_filter_close,
};

/*
 * Initialize the filter object.
 */
static int
brotli_bidder_init(struct archive_read_filter *f)
{
	struct brotli *brotli;
	void *out_block;
	BrotliDecoderState *state;

	f->code = ARCHIVE_FILTER_BROTLI;
	f->name = "brotli";

	brotli = calloc(1, sizeof(*brotli));
	out_block = malloc(BROTLI_OUT_BLOCK_SIZE);
	state = BrotliDecoderCreateInstance(NULL, NULL, NULL);
	if (brotli == NULL || out_block == NULL || state == NULL) {
		free(out_block);
		free(brotli);
		if (state != NULL)
			BrotliDecoderDestroyInstance(state);
		archive_set_error(&f->archive->archive, ENOMEM,
		    "Can't allocate data for Brotli decompression");
		return (ARCHIVE_FATAL);
	}

	brotli->state = state;
	brotli->out_block = out_block;
	f->data = brotli;
	f->vtable = &brotli_reader_vtable;

	return (ARCHIVE_OK);
}

static ssize_t
brotli_filter_read(struct archive_read_filter *f, const void **p)
{
	struct brotli *brotli = f->data;
	uint8_t *next_out = brotli->out_block;
	size_t avail_out = BROTLI_OUT_BLOCK_SIZE;

	/*
	 * Try to fill the output buffer. If something fails after some data
	 * was decompressed, return that data first: the decompressor fails
	 * again at the next call, with the same error.
	 */
	while (avail_out > 0 && !brotli->eof) {
		const uint8_t *next_in;
		size_t avail_in;
		ssize_t avail;
		BrotliDecoderResult result;

		next_in = __archive_read_filter_ahead(f->upstream, 1, &avail);
		if (avail < 0) {
			if (avail_out < BROTLI_OUT_BLOCK_SIZE)
				break;
			return (avail);
		}

		if (brotli->finished) {
			/* A stream ends on its own. Anything after it is not
			 * Brotli data. */
			if (next_in != NULL || avail > 0) {
				if (avail_out < BROTLI_OUT_BLOCK_SIZE)
					break;
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Unexpected data after the end of the "
				    "Brotli stream");
				return (ARCHIVE_FATAL);
			}
			brotli->eof = 1;
			break;
		}

		/*
		 * Even without more input, the decompressor may have more
		 * output to give.
		 */
		avail_in = (size_t)avail;
		result = BrotliDecoderDecompressStream(brotli->state,
		    &avail_in, &next_in, &avail_out, &next_out, NULL);

		/* The decompressor never takes more input than it needs. */
		__archive_read_filter_consume(f->upstream,
		    avail - (ssize_t)avail_in);

		switch (result) {
		case BROTLI_DECODER_RESULT_ERROR:
			if (avail_out < BROTLI_OUT_BLOCK_SIZE)
				goto done;
			archive_set_error(&f->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "Brotli decompression failed: %s",
			    BrotliDecoderErrorString(
				BrotliDecoderGetErrorCode(brotli->state)));
			return (ARCHIVE_FATAL);
		case BROTLI_DECODER_RESULT_SUCCESS:
			brotli->finished = 1;
			break;
		case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
			if (avail == 0) {
				/* The input ended, but not the stream. This
				 * includes empty input: the shortest stream
				 * has one byte. */
				if (avail_out < BROTLI_OUT_BLOCK_SIZE)
					goto done;
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Truncated Brotli input");
				return (ARCHIVE_FATAL);
			}
			break;
		case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
			/* The loop ends if the output buffer is full. */
			break;
		}
	}

done:
	if (avail_out == BROTLI_OUT_BLOCK_SIZE) {
		*p = NULL;
		return (0);
	}
	*p = brotli->out_block;
	return ((ssize_t)(BROTLI_OUT_BLOCK_SIZE - avail_out));
}

/*
 * Clean up the decompressor.
 */
static int
brotli_filter_close(struct archive_read_filter *f)
{
	struct brotli *brotli = f->data;

	BrotliDecoderDestroyInstance(brotli->state);
	free(brotli->out_block);
	free(brotli);

	return (ARCHIVE_OK);
}

#endif /* HAVE_BROTLI_DECODE_H && HAVE_LIBBROTLIDEC */
