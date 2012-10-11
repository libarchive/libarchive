/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
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
#include "archive_private.h"
#include "archive_read_private.h"

#define LZOP_HEADER_MAGIC "\x89\x4c\x5a\x4f\x00\x0d\x0a\x1a\x0a"
#define LZOP_HEADER_MAGIC_LEN 9

static int lzop_bidder_bid(struct archive_read_filter_bidder *,
    struct archive_read_filter *);
static int lzop_bidder_init(struct archive_read_filter *);
static int lzop_reader_free(struct archive_read_filter_bidder *);

int
archive_read_support_filter_lzop(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct archive_read_filter_bidder *reader;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_filter_lzop");

	if (__archive_read_get_bidder(a, &reader) != ARCHIVE_OK)
		return (ARCHIVE_FATAL);

	reader->data = NULL;
	reader->bid = lzop_bidder_bid;
	reader->init = lzop_bidder_init;
	reader->options = NULL;
	reader->free = lzop_reader_free;
	/* Return ARCHIVE_WARN since this always uses an external program. */
	archive_set_error(_a, ARCHIVE_ERRNO_MISC,
	    "Using external lzop program for lzop decompression");
	return (ARCHIVE_WARN);
}

static int
lzop_reader_free(struct archive_read_filter_bidder *self)
{
	(void)self; /* UNUSED */
	return (ARCHIVE_OK);
}

/*
 * Bidder just verifies the header and returns the number of verified bits.
 */
static int
lzop_bidder_bid(struct archive_read_filter_bidder *self,
    struct archive_read_filter *filter)
{
	const unsigned char *p;
	ssize_t avail;

	(void)self; /* UNUSED */

	p = __archive_read_filter_ahead(filter, LZOP_HEADER_MAGIC_LEN, &avail);
	if (p == NULL || avail == 0)
		return (0);

	if (memcmp(p, LZOP_HEADER_MAGIC, LZOP_HEADER_MAGIC_LEN))
		return (0);

	return (LZOP_HEADER_MAGIC_LEN * 8);
}

static int
lzop_bidder_init(struct archive_read_filter *self)
{
	int r;

	r = __archive_read_program(self, "lzop -d");
	/* Note: We set the format here even if __archive_read_program()
	 * above fails.  We do, after all, know what the format is
	 * even if we weren't able to read it. */
	self->code = ARCHIVE_FILTER_LZOP;
	self->name = "lzop";
	return (r);
}
