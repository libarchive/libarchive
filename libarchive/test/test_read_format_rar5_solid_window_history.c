/*-
 * Copyright (c) 2026 Tim Kientzle
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

/*
 * In a solid RAR5 archive, entries can declare different dictionary
 * sizes.  History from earlier entries must remain readable at the
 * right positions when the window grows or when a later entry
 * declares a smaller dictionary.
 */

static void
verify_entry(struct archive *a, const char *name, const char *expected)
{
	struct archive_entry *ae;
	char buf[64];
	ssize_t r;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(name, archive_entry_pathname(ae));
	r = archive_read_data(a, buf, sizeof(buf));
	assertEqualInt((ssize_t)strlen(expected), r);
	if (r > 0)
		assertEqualMem(buf, expected, (size_t)r);
	assertEqualInt(0, archive_read_data(a, buf, sizeof(buf)));
}

DEFINE_TEST(test_read_format_rar5_solid_window_history)
{
	const char *reffile = "test_read_format_rar5_solid_window_history.rar";
	struct archive_entry *ae;
	struct archive *a;
	char buf[4096];
	ssize_t r;
	size_t total = 0;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));

	/* 200000 bytes of "ABCDEFGHIJKLMNOPQ" repeated. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("first.bin", archive_entry_pathname(ae));
	while ((r = archive_read_data(a, buf, sizeof(buf))) > 0)
		total += (size_t)r;
	assertEqualInt(0, r);
	assertEqualInt(200000, total);

	/* Dictionary grows from 128 KiB to 256 KiB. */
	verify_entry(a, "grow.bin", "CDEFGHIJKLCDEFGHI");

	/* Dictionary declared as 128 KiB again. */
	verify_entry(a, "shrink.bin", "PQABCDEFGHIJKLCDE");

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Same problem with an archive made by RAR itself: appending to a
 * solid archive with a smaller -md switch gives a solid stream whose
 * dictionary size shrinks.
 */
DEFINE_TEST(test_read_format_rar5_solid_window_shrink)
{
	const char *reffile = "test_read_format_rar5_solid_window_shrink.rar";
	struct archive_entry *ae;
	struct archive *a;
	char *first, *second;
	ssize_t r;
	size_t total;

	first = malloc(200000 + 1);
	second = malloc(797 + 1);
	assert(first != NULL && second != NULL);
	if (first == NULL || second == NULL) {
		free(first);
		free(second);
		return;
	}

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("a.txt", archive_entry_pathname(ae));
	total = 0;
	while ((r = archive_read_data(a, first + total, 200000 + 1 - total)) > 0)
		total += (size_t)r;
	assertEqualInt(0, r);
	assertEqualInt(200000, total);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("b.txt", archive_entry_pathname(ae));
	total = 0;
	while ((r = archive_read_data(a, second + total, 797 + 1 - total)) > 0)
		total += (size_t)r;
	assertEqualInt(0, r);
	assertEqualInt(797, total);

	/* b.txt is bytes 500..996 and then 0..299 of a.txt. */
	if (total == 797) {
		assertEqualMem(second, first + 500, 497);
		assertEqualMem(second + 497, first, 300);
	}

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(first);
	free(second);
}
