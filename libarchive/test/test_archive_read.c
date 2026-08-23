/*-
 * Copyright (c) 2024 Tobias Stoeckmann
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

#include "test.h"

#define __LIBARCHIVE_TEST
#include "archive_read_private.h"

static char buf[1024];

DEFINE_TEST(test_archive_read_ahead_eof)
{
	struct archive *a;
	struct archive_read *ar;
	ssize_t avail;

	/* prepare a reader of raw in-memory data */
	assert((a = archive_read_new()) != NULL);
	ar = (struct archive_read *)a;

	assertA(0 == archive_read_support_format_raw(a));
	assertA(0 == archive_read_open_memory(a, buf, sizeof(buf)));

	/* perform a read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, sizeof(buf) - 1, &avail));
	assertEqualInt(sizeof(buf), avail);

	/* perform a read which cannot be fulfilled due to EOF */
	assert(NULL == __archive_read_ahead(ar, sizeof(buf) + 1, &avail));
	assertEqualInt(sizeof(buf), avail);

	/* perform the same read again */
	assert(NULL == __archive_read_ahead(ar, sizeof(buf) + 1, &avail));
	assertEqualInt(sizeof(buf), avail);

	/* perform another read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, sizeof(buf), &avail));
	assertEqualInt(sizeof(buf), avail);

	assert(0 == archive_read_free(a));
}

DEFINE_TEST(test_archive_read_ahead_zero)
{
	struct archive *a;
	struct archive_read *ar;
	ssize_t avail;

	/* prepare a reader of raw in-memory data */
	assert((a = archive_read_new()) != NULL);
	ar = (struct archive_read *)a;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, buf, sizeof(buf)));

	/* perform a zero read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, 0, &avail));
	assertEqualIntA(ar, sizeof(buf), avail);

	/* perform a read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, 1, &avail));
	assertEqualIntA(ar, sizeof(buf), avail);

	/* perform the zero read again */
	assert(NULL != __archive_read_ahead(ar, 0, &avail));
	assertEqualIntA(ar, sizeof(buf), avail);

	/* consume all available bytes */
	assertEqualIntA(ar, sizeof(buf),
	    __archive_read_consume(ar, sizeof(buf)));

	/* perform a read which cannot be fulfilled */
	assert(NULL == __archive_read_ahead(ar, 1, &avail));
	assertEqualIntA(ar, 0, avail);

	/* perform another zero read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, 0, &avail));
	assertEqualIntA(ar, 0, avail);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* prepare a reader of raw in-memory data */
	assert((a = archive_read_new()) != NULL);
	ar = (struct archive_read *)a;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_empty(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, buf, 0));

	/* perform a zero read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, 0, &avail));
	assertEqualIntA(ar, 0, avail);

	/* perform a read which cannot be fulfilled */
	assert(NULL == __archive_read_ahead(ar, 1, &avail));
	assertEqualIntA(ar, 0, avail);

	/* perform another zero read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, 0, &avail));
	assertEqualIntA(ar, 0, avail);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static ssize_t
eof_read_callback(struct archive *a, void *data, const void **buffer)
{
	(void)a;
	(void)data;
	(void)buffer;

	return (0);
}

static int
evil_open_callback(struct archive *a, void *data)
{
	(void)data;

	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_set_read_callback(a, NULL));
	return (ARCHIVE_OK);
}

DEFINE_TEST(test_archive_read_state_open)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Prepare callbacks which modify callbacks when used. */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_open_callback(a, evil_open_callback));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_read_callback(a, eof_read_callback));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_callback_data(a, NULL));

	/* Try to open archive. */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_open1(a));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
