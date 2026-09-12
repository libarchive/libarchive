/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

static unsigned char nulls[1000];
static unsigned char buff[32768];
static size_t data_sizes[] = {0, 5, 511, 512, 513};

/* Check that archive_write_header_position() tracks correctly on write. */
DEFINE_TEST(test_write_position)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t write_pos;
	size_t i;
	int64_t expected_pos;
	int64_t write_positions[sizeof(data_sizes)/sizeof(data_sizes[0])];

	assert(sizeof(nulls) + 512 <= sizeof(buff));

	/* Create an archive and record positions at each archive_write_header(). */
	assert(NULL != (a = archive_write_new()));
	assertA(0 == archive_write_set_format_pax_restricted(a));
	assertA(0 == archive_write_set_bytes_per_block(a, 512));
	assertA(0 == archive_write_open_memory(a, buff, sizeof(buff), &write_pos));

	expected_pos = 0;
	for (i = 0; i < sizeof(data_sizes)/sizeof(data_sizes[0]); ++i) {
		assert((ae = archive_entry_new()) != NULL);
		archive_entry_set_pathname(ae, "testfile");
		archive_entry_set_mode(ae, S_IFREG);
		archive_entry_set_size(ae, data_sizes[i]);
		assertA(0 == archive_write_header(a, ae));
		archive_entry_free(ae);

		/* Position captured after archive_write_header() should equal
		 * expected byte offset of this header in the format stream. */
		assertEqualInt(expected_pos,
		    (intmax_t)archive_write_header_position(a));
		write_positions[i] = archive_write_header_position(a);

		assertA(data_sizes[i]
		    == (size_t)archive_write_data(a, nulls, sizeof(nulls)));

		expected_pos += 512; /* header block */
		expected_pos += (data_sizes[i] + 511) & ~511; /* data blocks */
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * Read the archive back and verify write positions match read positions.
	 */
	assert(NULL != (a = archive_read_new()));
	assertA(0 == archive_read_support_format_tar(a));
	assertA(0 == read_open_memory(a, buff, sizeof(buff), 512));

	for (i = 0; i < sizeof(data_sizes)/sizeof(data_sizes[0]); ++i) {
		assertA(0 == archive_read_next_header(a, &ae));
		assertEqualInt(write_positions[i],
		    (intmax_t)archive_read_header_position(a));
		assertA(0 == archive_read_data_skip(a));
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	archive_read_free(a);
}
