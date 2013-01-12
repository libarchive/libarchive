/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2011-2012 Michihiro NAKAJIMA
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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_format_raw.c 201170 2009-12-29 06:34:23Z kientzle $");

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

#include "archive.h"
#include "archive_entry.h"
#include "archive_entry_locale.h"
#include "archive_private.h"
#include "archive_write_private.h"

static ssize_t	archive_write_raw_data(struct archive_write *,
		    const void *buff, size_t s);
static int	archive_write_raw_close(struct archive_write *);
static int	archive_write_raw_free(struct archive_write *);
static int	archive_write_raw_finish_entry(struct archive_write *);
static int	archive_write_raw_header(struct archive_write *,
		    struct archive_entry *);

struct raw {
        int entries_written;
};

/*
 * Set output format to 'raw' format.
 */
int
archive_write_set_format_raw(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct raw *raw;

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_format_raw");

	/* If someone else was already registered, unregister them. */
	if (a->format_free != NULL)
		(a->format_free)(a);

	raw = (struct raw *)calloc(1, sizeof(*raw));
	if (raw == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate raw data");
		return (ARCHIVE_FATAL);
	}
	raw->entries_written = 0;
	a->format_data = raw;
	a->format_name = "raw";
	a->format_write_header = archive_write_raw_header;
	a->format_write_data = archive_write_raw_data;
	a->format_finish_entry = archive_write_raw_finish_entry;
	a->format_close = archive_write_raw_close;
	a->format_free = archive_write_raw_free;
	a->archive.archive_format = ARCHIVE_FORMAT_RAW;
	a->archive.archive_format_name = "RAW";
	return (ARCHIVE_OK);
}

static int
archive_write_raw_header(struct archive_write *a, struct archive_entry __attribute__((unused))*entry)
{
	struct raw *raw = (struct raw *)a->format_data;
	if (raw->entries_written > 1) {
		archive_set_error(&a->archive, ERANGE,
		    "Too many files for the raw format");
		return (ARCHIVE_FATAL);
	}

	//return write_header(a, entry);
	return (ARCHIVE_OK);
}

static ssize_t
archive_write_raw_data(struct archive_write *a, const void *buff, size_t s)
{
	int ret;

	ret = __archive_write_output(a, buff, s);
	if (ret >= 0)
		return (s);
	else
		return (ret);
}

static int
archive_write_raw_close(struct archive_write __attribute__((unused))*a)
{
	return (ARCHIVE_OK);
}

static int
archive_write_raw_free(struct archive_write *a)
{
	struct raw *raw;

	raw = (struct raw *)a->format_data;
	free(raw);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

static int
archive_write_raw_finish_entry(struct archive_write *a)
{
	struct raw *raw;

	raw = (struct raw *)a->format_data;
	raw->entries_written++;
	return (ARCHIVE_OK);
}
