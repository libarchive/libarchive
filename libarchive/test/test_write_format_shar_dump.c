/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Martin Mirchev
 * All rights reserved.
 */
#include "test.h"

/*
 * The shar dump writer buffers partial 45-byte uuencode lines between
 * writes.  Check that the data reads back unchanged when a write
 * completes a buffered line and leaves nothing (or only whole lines)
 * behind.
 */

static void
write_and_verify(const char *data, size_t size, const size_t *chunks)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[4096], rbuff[256];
	size_t used, offset;
	ssize_t r;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_shar_dump(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, size);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	for (offset = 0; *chunks != 0; offset += *chunks++)
		assertEqualIntA(a, (int)*chunks,
		    (int)archive_write_data(a, data + offset, *chunks));
	assertEqualInt(size, offset);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Decode the uuencoded data with the uu filter. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_uu(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	r = archive_read_data(a, rbuff, sizeof(rbuff));
	assertEqualInt((ssize_t)size, r);
	if (r > 0)
		assertEqualMem(data, rbuff, size);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_write_format_shar_dump_line_boundary)
{
	/* Each list of write sizes ends with 0. */
	static const size_t exact_line[] = { 44, 1, 0 };
	static const size_t whole_lines[] = { 44, 46, 0 };
	static const size_t next_write[] = { 44, 1, 10, 0 };
	static const size_t partial[] = { 44, 2, 0 };
	char data[100];
	size_t i;

	for (i = 0; i < sizeof(data); i++)
		data[i] = (char)(i * 7 + 1);

	write_and_verify(data, 45, exact_line);
	write_and_verify(data, 90, whole_lines);
	write_and_verify(data, 55, next_write);
	write_and_verify(data, 46, partial);
}
