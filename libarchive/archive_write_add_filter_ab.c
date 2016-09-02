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

/*
 * The file order of the archive is important
 * It is described here: https://android.googlesource.com/platform/frameworks/base/+/4a627c71ff53a4fca1f961f4b1dcc0461df18a06
 *
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
#include <time.h>
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_string.h"
#include "archive_write_private.h"

static const char AB_MAGIC[] = "ANDROID BACKUP";

struct private_data {
	/* for header */
	char version;
	char compression_level;
	/* misc */
	char has_header;
	int64_t total_in;
#if HAVE_ZLIB_H
	z_stream stream;
	void* out_block;
	size_t out_block_size;
#endif
};

static inline int write_ab_header(struct archive_write_filter* f)
{
	struct private_data* data = (struct private_data*)f->data;
	char buffer[64];
	size_t bw;
	int ret;

	bw = snprintf(buffer, sizeof(buffer), "%s\n%d\n%d\n%s\n", AB_MAGIC, data->version, (data->compression_level ? 1 : 0), "none");
	if(bw > sizeof(buffer) - 1)
		bw = sizeof(buffer) - 1;

	ret = __archive_write_filter(f->next_filter, buffer, bw);
	if(ret < ARCHIVE_OK)
		return ret;

	data->has_header = 1;
	return ARCHIVE_OK;
}

static int archive_write_ab_options(struct archive_write_filter* f, const char* key, const char* value)
{
	struct private_data* data = (struct private_data*)f->data;
	int val = 0;

	if (strcmp(key, "compression-level") == 0)
	{
		if (value == NULL || !((val = value[0] - '0') >= 0 && val <= 9) || value[1] != '\0')
			return ARCHIVE_WARN;

#if !(HAVE_ZLIB_H)
		if(val != 0)
		{
			archive_set_error(f->archive, ARCHIVE_ERRNO_PROGRAMMER, "ab: DEFLATE compression not supported in this build");
			return ARCHIVE_FATAL;
		}
#endif

		data->compression_level = val;
		return ARCHIVE_OK;
	}

	if (strcmp(key, "version") == 0) 
	{
		if (value == NULL || !((val = value[0] - '0') >= 1 && val <= 3) || value[1] != '\0')
			return ARCHIVE_WARN;

		data->version = val;
		return ARCHIVE_OK;
	}

	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return ARCHIVE_WARN;
}

#if HAVE_ZLIB_H
/* based on drive_compressor from archive_write_add_filter_gzip.c */
static int drive_compressor(struct archive_write_filter* f, struct private_data* data, int finishing)
{
	int ret;

	for (;;) 
	{
		if (data->stream.avail_out == 0) 
		{
			ret = __archive_write_filter(f->next_filter, data->out_block, data->out_block_size);
			if (ret != ARCHIVE_OK)
				return ARCHIVE_FATAL;
			data->stream.next_out = data->out_block;
			data->stream.avail_out = (uInt)data->out_block_size;
		}

		/* If there's nothing to do, we're done. */
		if (!finishing && data->stream.avail_in == 0)
			return ARCHIVE_OK;

		ret = deflate(&(data->stream),
		    finishing ? Z_FINISH : Z_NO_FLUSH );

		switch (ret) 
		{
		case Z_OK:
			/* In non-finishing case, check if compressor
			 * consumed everything */
			if (!finishing && data->stream.avail_in == 0)
				return ARCHIVE_OK;
			/* In finishing case, this return always means
			 * there's more work */
			break;
		case Z_STREAM_END:
			/* This return can only occur in finishing case. */
			return ARCHIVE_OK;
		default:
			/* Any other return value indicates an error. */
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "DEFLATE compression failed: deflate() call returned status %d",
			    ret);
			return ARCHIVE_FATAL;
		}
	}
}
#endif

static inline int archive_write_ab_write_deflate(struct archive_write_filter* f, const void* buff, size_t length)
{
#if HAVE_ZLIB_H
	struct private_data* data = (struct private_data*)f->data;
	int ret;

	/* Compress input data to output buffer */
	data->stream.next_in = (Bytef*)buff;
	data->stream.avail_in = (uInt)length;
	if ((ret = drive_compressor(f, data, 0)) != ARCHIVE_OK)
		return ret;

	return ARCHIVE_OK;
#else
	(void)f; /* UNUSED */
	(void)buff; /* UNUSED */
	(void)length; /* UNUSED */

	return ARCHIVE_FATAL;
#endif
}

static inline int archive_write_ab_write_raw(struct archive_write_filter* f, const void* buff, size_t length)
{
	return __archive_write_filter(f->next_filter, buff, length);
}

static int archive_write_ab_write(struct archive_write_filter* f, const void* buff, size_t length)
{
	struct private_data* data = (struct private_data*)f->data;

	if(!data->has_header)
		write_ab_header(f);

	data->total_in += length;

	if(data->compression_level)
		return archive_write_ab_write_deflate(f, buff, length);
	else
		return archive_write_ab_write_raw(f, buff, length);
}

static int archive_write_ab_open(struct archive_write_filter* f)
{
	struct private_data* data = (struct private_data*)f->data;
	int ret;

	ret = __archive_write_open_filter(f->next_filter);
	if (ret != ARCHIVE_OK)
		return ret;

#if HAVE_ZLIB_H
	if(data->compression_level)
	{
		if (data->out_block == NULL) 
		{
			size_t bs = 65536, bpb;
			if (f->archive->magic == ARCHIVE_WRITE_MAGIC) 
			{
				/* Buffer size should be a multiple number of
				 * the of bytes per block for performance. */
				bpb = archive_write_get_bytes_per_block(f->archive);
				if (bpb > bs)
					bs = bpb;
				else if (bpb != 0)
					bs -= bs % bpb;
			}

			data->out_block_size = bs;
			data->out_block = malloc(bs);
			if (data->out_block == NULL)
			{
				archive_set_error(f->archive, ENOMEM,
					"Can't allocate data for compression buffer");
				return ARCHIVE_FATAL;
			}
		}

		data->stream.next_out = data->out_block;
		data->stream.avail_out = (uInt)data->out_block_size;

		/* Initialize compression library. */
		ret = deflateInit2(&(data->stream),
			((data->compression_level == (char)Z_DEFAULT_COMPRESSION) ? Z_DEFAULT_COMPRESSION : data->compression_level),
			Z_DEFLATED,
			15,
			8,
			Z_DEFAULT_STRATEGY);

		if (ret == Z_OK)
			goto End;

		/* Library setup failed: clean up. */
		archive_set_error(f->archive, ARCHIVE_ERRNO_MISC, "Internal error initializing zlib compression library");

		/* Override the error message if we know what really went wrong. */
		switch (ret)
		{
		case Z_STREAM_ERROR:
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
				"Internal error initializing zlib compression library: invalid setup parameter");
			break;
		case Z_MEM_ERROR:
			archive_set_error(f->archive, ENOMEM,
				"Internal error initializing zlib compression library");
			break;
		case Z_VERSION_ERROR:
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
				"Internal error initializing zlib compression library: invalid library version");
			break;
		}

		return ARCHIVE_FATAL;
	}

End:
#endif
	f->write = archive_write_ab_write;
	f->data = data;
	return ARCHIVE_OK;
}

static int archive_write_ab_close(struct archive_write_filter* f)
{
	int ret = 0, r1;
#if HAVE_ZLIB_H
	struct private_data* data = (struct private_data*)f->data;

	if(data->compression_level)
	{
		/* Finish compression cycle */
		ret = drive_compressor(f, data, 1);
		if (ret == ARCHIVE_OK) 
		{
			/* Write the last compressed data. */
			ret = __archive_write_filter(f->next_filter,
				data->out_block,
				data->out_block_size - data->stream.avail_out);
		}

		switch (deflateEnd(&(data->stream)))
		{
		case Z_OK:
			break;
		default:
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
				"Failed to clean up zlib compressor");
			ret = ARCHIVE_FATAL;
		}
	}
#endif
	r1 = __archive_write_close_filter(f->next_filter);
		return (r1 < ret ? r1 : ret);
}

static int archive_write_ab_free(struct archive_write_filter* f)
{
	struct private_data* data = (struct private_data*)f->data;

#if HAVE_ZLIB_H
	free(data->out_block);
#endif
	free(data);
	f->data = NULL;
	return ARCHIVE_OK;
}

int archive_write_add_filter_ab(struct archive* _a)
{
	struct archive_write* a = (struct archive_write*)_a;
	struct archive_write_filter* f = __archive_write_allocate_filter(_a);
	struct private_data* data;

	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_ab");

	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Out of memory");
		return ARCHIVE_FATAL;
	}

	f->data = data;
	f->open = &archive_write_ab_open;
	f->options = &archive_write_ab_options;
	f->close = &archive_write_ab_close;
	f->free = &archive_write_ab_free;

	f->code = ARCHIVE_FILTER_AB;
	f->name = "Android Backup";

	data->has_header = 0;
	data->version = 3;
#if HAVE_ZLIB_H
	data->compression_level = Z_DEFAULT_COMPRESSION;
#else
	data->compression_level = 0;
#endif
	return ARCHIVE_OK;
}
