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

/* A gzip stream of "foo\n" that stores the name "stored.txt". */
static const unsigned char gzip_with_name[] = {
	0x1f, 0x8b, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
	0x73, 0x74, 0x6f, 0x72, 0x65, 0x64, 0x2e, 0x74, 0x78, 0x74,
	0x00, 0x4b, 0xcb, 0xcf, 0xe7, 0x02, 0x00, 0xa8, 0x65, 0x32,
	0x7e, 0x04, 0x00, 0x00, 0x00
};

static void
check_name(struct archive *a, const void *data, size_t size,
    const char *expected)
{
	char buff[16];
	struct archive_entry *ae;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, data, size));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(expected, archive_entry_pathname(ae));
	assertEqualInt(4, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "foo\n", 4);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_raw_pathname)
{
	static const char data[] = "foo\n";
	struct archive *a;

	/* The default name. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	check_name(a, data, sizeof(data) - 1, "data");

	/* The name can be set with the options of the reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_options(a, "raw:pathname=report.txt"));
	check_name(a, data, sizeof(data) - 1, "report.txt");

	/* ... or with the option of the format. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_format_option(a, "raw", "pathname", "report.txt"));
	check_name(a, data, sizeof(data) - 1, "report.txt");

	/* The last setting wins, and an empty one restores the default. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_format_option(a, "raw", "pathname", "first.txt"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_format_option(a, "raw", "pathname", "second.txt"));
	check_name(a, data, sizeof(data) - 1, "second.txt");
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_format_option(a, "raw", "pathname", "first.txt"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_format_option(a, "raw", "pathname", ""));
	check_name(a, data, sizeof(data) - 1, "data");

	/* An unknown option is still reported. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_read_set_format_option(a, "raw", "unknown", "x"));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* A name stored in the data is used in preference to the one that was set. */
DEFINE_TEST(test_read_format_raw_pathname_stored)
{
	struct archive *a;

	if (archive_zlib_version() == NULL) {
		skipping("gzip support is not available");
		return;
	}

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_gzip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	check_name(a, gzip_with_name, sizeof(gzip_with_name), "stored.txt");

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_gzip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_format_option(a, "raw", "pathname", "report.txt"));
	check_name(a, gzip_with_name, sizeof(gzip_with_name), "stored.txt");
}
