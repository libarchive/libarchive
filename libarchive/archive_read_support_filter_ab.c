/*-
 * Copyright (c) 2016 dosomder
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
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

static const char AB_MAGIC[] = "ANDROID BACKUP";

enum AB_HEADER_FIELD {
	AB_HEADER_MAGIC = 0,
	AB_HEADER_VERSION,
	AB_HEADER_COMPRESSION,
	AB_HEADER_ENCRYPTION,
	AB_HEADER_END,
};

struct private_data {
	/* from header */
	char version;
	char compressed;
	/* misc */
	char in_stream;
	char eof;
	int64_t total_out;
#if HAVE_ZLIB_H
	z_stream stream;
	void* out_block;
	size_t out_block_size;
#endif
};

static inline char* getnextline(const char* str)
{
	char* nline = strchr(str, '\n');
	return (nline == NULL) ? NULL : (nline + 1);
}

static int read_str_ahead(struct archive_read_filter* readfilter, char* buffer, size_t len)
{
	const void* ptr = __archive_read_filter_ahead(readfilter, len, NULL);
	if(ptr == NULL)
		return -1;

	memcpy(buffer, ptr, len);
	buffer[len - 1] = 0;

	return 0;
}

static char* parse_ab_header(const char* header, enum AB_HEADER_FIELD field)
{
	char* l = (char*)header;
	enum AB_HEADER_FIELD curfield = AB_HEADER_MAGIC;

	do
	{
		if(field == curfield)
			return l;
	} while (curfield++ < AB_HEADER_END && (l = getnextline(l)) != NULL);

	return NULL;
}

static int ab_reader_bid(struct archive_read_filter_bidder* self, struct archive_read_filter* filter)
{
	char h[64];
	char* l = h;

	(void)self; /* UNUSED */

	/* Now let's look at the actual header and see if it matches. */
	if(read_str_ahead(filter, h, 64) != 0)
		return 0;

	if(memcmp(AB_MAGIC, h, strlen(AB_MAGIC)) != 0)
		return 0;

	if((l = getnextline(h)) == NULL)
		return 0;

	/* version 1 to 3 supported */
	if(l[0] < '1' || l[0] > '3')
		return 0;

	if((l = getnextline(l)) == NULL)
		return 0;

	/* compression true or false */
	if(l[0] < '0' || l[0] > '1')
		return 0;

	if((l = getnextline(l)) == NULL)
		return 0;

	/* encryption method none or AES-256 (not supported) */
	if(memcmp(l, "none", 4) != 0)
		return 0;

	if((l = getnextline(l)) == NULL)
		return 0;

	return ((l - h) << 3);
}

static inline ssize_t ab_filter_read_raw(struct archive_read_filter* self, const void** p)
{
	const void* data;
	ssize_t datalen;

	data = __archive_read_filter_ahead(self->upstream, 1, &datalen);
	if(data == NULL)
		return datalen;

	*p = data;
	__archive_read_filter_consume(self->upstream, datalen);
	return datalen;
}

static inline int ab_zlib_init(struct archive_read_filter* self)
{
#if HAVE_ZLIB_H
	int ret;
	struct private_data* state;

	state = (struct private_data*)self->data;
	ssize_t avail = 0;

	state->stream.next_in = (Bytef*)__archive_read_filter_ahead(self->upstream, 1, &avail);
	state->stream.avail_in = (uInt)avail;
	ret = inflateInit2(&(state->stream), 0);

	/* Decipher the error code. */
	switch (ret) {
	case Z_OK:
		state->in_stream = 1;
		return ARCHIVE_OK;
	case Z_STREAM_ERROR:
		archive_set_error(&self->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid setup parameter");
		break;
	case Z_MEM_ERROR:
		archive_set_error(&self->archive->archive, ENOMEM,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	case Z_VERSION_ERROR:
		archive_set_error(&self->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid library version");
		break;
	default:
		archive_set_error(&self->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    " Zlib error %d", ret);
		break;
	}
#else
	(void)self; /* UNUSED */
#endif
	return ARCHIVE_FATAL;
}

static inline int ab_zlib_destroy(struct archive_read_filter* self)
{
#if HAVE_ZLIB_H
	struct private_data* state;

	state = (struct private_data*)self->data;

	state->in_stream = 0;
	switch (inflateEnd(&(state->stream))) {
	case Z_OK:
		return ARCHIVE_OK;
	default:
		archive_set_error(&self->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Failed to clean up zlib decompressor");
		return ARCHIVE_FATAL;
	}
#else
	(void)self; /* UNUSED */
#endif
	return ARCHIVE_FATAL;
}

/* based on gzip_filter_read */
static inline ssize_t ab_filter_read_deflate(struct archive_read_filter* self, const void** p)
{
#if HAVE_ZLIB_H
	struct private_data* state;
	size_t decompressed;
	ssize_t avail_in;
	int ret;

	state = (struct private_data*)self->data;

	/* Empty our output buffer. */
	state->stream.next_out = state->out_block;
	state->stream.avail_out = (uInt)state->out_block_size;

	/* Try to fill the output buffer. */
	while (state->stream.avail_out > 0 && !state->eof)
	{
		/* Peek at the next available data. */
		state->stream.next_in = (Bytef*)__archive_read_filter_ahead(self->upstream, 1, &avail_in);
		if (state->stream.next_in == NULL) {
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "ab: truncated deflate input");
			return ARCHIVE_FATAL;
		}
		state->stream.avail_in = (uInt)avail_in;

		/* Decompress and consume some of that data. */
		ret = inflate(&(state->stream), 0);
		switch (ret) {
		case Z_OK: /* Decompressor made some progress. */
			__archive_read_filter_consume(self->upstream,
			    avail_in - state->stream.avail_in);
			break;
		case Z_STREAM_END: /* Found end of stream. */
			state->eof = 1;
			__archive_read_filter_consume(self->upstream,
			    avail_in - state->stream.avail_in);
			ret = ab_zlib_destroy(self);
			if (ret < ARCHIVE_OK)
				return ret;
			break;
		default:
			/* Return an error. */
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "ab: deflate decompression failed");
			return ARCHIVE_FATAL;
		}
	}

	/* We've read as much as we can. */
	decompressed = (void*)state->stream.next_out - state->out_block;
	state->total_out += decompressed;
	if (decompressed == 0)
		*p = NULL;
	else
		*p = state->out_block;
	return decompressed;
#else
	(void)self; /* UNUSED */
	(void)p; /* UNUSED */

	return ARCHIVE_FATAL;
#endif
}

static inline int consume_header(struct archive_read_filter* self)
{
	char h[64];
	char* l = h;

	/* determine header length */
	if(read_str_ahead(self->upstream, h, 64) != 0 || (l = parse_ab_header(h, AB_HEADER_END)) == NULL)
	{
		archive_set_error(&self->archive->archive, ARCHIVE_ERRNO_MISC,
			"ab: reading failed");
		return ARCHIVE_FATAL;
	}

	__archive_read_filter_consume(self->upstream, (l - h));
	return ARCHIVE_OK;
}

static ssize_t ab_filter_read(struct archive_read_filter* self, const void** p)
{
	struct private_data* state;

	state = (struct private_data*)self->data;
	*p = NULL;

	if(!state->in_stream)
	{
		int ret;
		if((ret = consume_header(self)) < ARCHIVE_OK)
			return ret;
		if(state->compressed)
			if((ret = ab_zlib_init(self)) < ARCHIVE_OK)
				return ret;

		state->in_stream = 1;
	}

	if(state->compressed)
		return ab_filter_read_deflate(self, p);
	else
		return ab_filter_read_raw(self, p);
}

/*
 * Clean up the decompressor.
 */
static int ab_filter_close(struct archive_read_filter* self)
{
	struct private_data* state;
	int ret;

	state = (struct private_data*)self->data;
	ret = ARCHIVE_OK;

#if HAVE_ZLIB_H
	if(state->compressed)
	{
		if (state->in_stream) {
			switch (inflateEnd(&(state->stream))) {
			case Z_OK:
				break;
			default:
				archive_set_error(&(self->archive->archive),
					ARCHIVE_ERRNO_MISC,
					"failed to clean up ab compressor");
				ret = ARCHIVE_FATAL;
			}
		}
	}
#endif

#if HAVE_ZLIB_H
	free(state->out_block);
#endif
	free(state);
	return ret;
}

static int ab_reader_init(struct archive_read_filter* self)
{
	struct private_data* state;
	char h[64];
#if HAVE_ZLIB_H
	static const size_t out_block_size = 64 * 1024;
	void* out_block;
#endif

	self->code = ARCHIVE_FILTER_AB;
	self->name = "Android Backup";

	state = (struct private_data*)calloc(1, sizeof(*state));
#if HAVE_ZLIB_H
	out_block = malloc(out_block_size);
	if (state == NULL || out_block == NULL) 
	{
		free(out_block);
#else
	if (state == NULL)
	{
#endif
		free(state);
		archive_set_error(&self->archive->archive, ENOMEM,
		    "Can't allocate data for ab decompression");
		return ARCHIVE_FATAL;
	}

	if(read_str_ahead(self->upstream, h, 64) != 0)
	{
		archive_set_error(&self->archive->archive, ARCHIVE_ERRNO_MISC,
		    "ab: Unable to read archive");
		return ARCHIVE_FATAL;
	}

	self->data = state;
	{
		char* ver = parse_ab_header(h, AB_HEADER_VERSION);
		char* comp = parse_ab_header(h, AB_HEADER_COMPRESSION);
		if(!ver || !comp)
		{
			archive_set_error(&self->archive->archive, ARCHIVE_ERRNO_MISC,
				"ab: Unable to read archive");
			return ARCHIVE_FATAL;
		}
		state->version = ver[0] - '0';
		state->compressed = comp[0] - '0';
	}

#if HAVE_ZLIB_H
	state->out_block_size = out_block_size;
	state->out_block = out_block;
#endif

#if !(HAVE_ZLIB_H)
	if(state->compressed)
	{
		archive_set_error(&self->archive->archive, ARCHIVE_ERRNO_MISC,
		    "ab: DEFLATE compression not supported in this build");
		return ARCHIVE_FATAL;
	}
#endif

	self->read = ab_filter_read;
	self->skip = NULL; /* not supported */
	self->close = ab_filter_close;

	state->in_stream = 0; /* We're not actually within a stream yet. */

	return ARCHIVE_OK;
}

int archive_read_support_filter_ab(struct archive* _a)
{
	struct archive_read* a = (struct archive_read*)_a;
	struct archive_read_filter_bidder* bidder;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_filter_ab");

	if (__archive_read_get_bidder(a, &bidder) != ARCHIVE_OK)
		return ARCHIVE_FATAL;

	bidder->data = NULL;
	bidder->name = "Android Backup";
	bidder->bid = ab_reader_bid;
	bidder->init = ab_reader_init;
	bidder->options = NULL;
	bidder->free = NULL; /* No data, so no cleanup necessary. */

	return ARCHIVE_OK;
}
