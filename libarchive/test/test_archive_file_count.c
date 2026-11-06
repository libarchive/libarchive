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
#include "test.h"
#include <limits.h>
#define __LIBARCHIVE_TEST 1
#include "archive_private.h"

/* Writes an archive of one file to buff, and returns its size. */
static size_t
make_archive(char *buff, size_t size)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t used = 0;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, size, &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	return (used);
}

DEFINE_TEST(test_archive_file_count)
{
	char buff[4096];
	struct archive *a;
	struct archive_entry *ae;
	size_t used;

	/* The count does not wrap when it does not fit in an int. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(0, archive_file_count(a));
	a->file_count = INT_MAX;
	assertEqualInt(INT_MAX, archive_file_count(a));
	a->file_count = (int64_t)INT_MAX + 1;
	assertEqualInt(INT_MAX, archive_file_count(a));
	a->file_count = INT64_MAX;
	assertEqualInt(INT_MAX, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Reading a header after that many does not overflow. */
	used = make_archive(buff, sizeof(buff));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	a->file_count = INT_MAX;
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((int64_t)INT_MAX + 1, a->file_count);
	assertEqualInt(INT_MAX, archive_file_count(a));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt((int64_t)INT_MAX + 1, a->file_count);
	assertEqualInt(INT_MAX, archive_file_count(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Neither does writing one. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	a->file_count = INT_MAX;
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt((int64_t)INT_MAX + 1, a->file_count);
	assertEqualInt(INT_MAX, archive_file_count(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}
