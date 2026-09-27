/*-
 * Copyright (c) 2026 Kaixuan Li
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
 * Regression test for a no-forward-progress loop in the RAR5 skip path.
 *
 * archive_read_data_skip() returns the reader to ARCHIVE_STATE_HEADER for
 * anything short of ARCHIVE_FATAL, so a caller that reports the error and
 * asks for the next header expects the failed entry to be behind it.
 *
 * On the solid path rar5_read_data_skip() forwarded ARCHIVE_FAILED from
 * parse_block_header() without consuming input or reducing bytes_remaining,
 * so the caller re-read the same bytes forever.
 *
 * Reproducer: the first 108 bytes of
 * test_read_format_rar5_multiple_files_solid.rar.
 */
DEFINE_TEST(test_read_format_rar5_skip_no_progress)
{
	/* Truncated RAR5 solid archive. */
	static const uint8_t data[] = {
		0x52,0x61,0x72,0x21,0x1a,0x07,0x01,0x00,
		0x09,0xef,0xc8,0x6f,0x0b,0x01,0x05,0x07,
		0x04,0x06,0x01,0x01,0x80,0x80,0x80,0x00,
		0x19,0x8c,0x94,0xff,0x27,0x02,0x03,0x0b,
		0xf9,0x02,0x04,0x80,0x20,0xa4,0x83,0x02,
		0xc6,0xb2,0x13,0x7e,0x80,0x1d,0x01,0x09,
		0x74,0x65,0x73,0x74,0x31,0x2e,0x62,0x69,
		0x6e,0x0a,0x03,0x13,0x67,0x5f,0xac,0x5b,
		0x1a,0x5a,0x9e,0x10,0xc9,0xe7,0x75,0x01,
		0x18,0x65,0x54,0x65,0x26,0xf4,0x80,0x57,
		0xf5,0xf3,0xe7,0xcf,0x92,0x49,0x24,0x92,
		0x49,0x24,0x92,0x49,0x24,0x92,0x49,0x24,
		0x92,0x49,0x24,0x92,0x49,0x24,0x92,0x49,
		0x24,0x92,0x49,0x24
	};

	struct archive *a;
	struct archive_entry *ae;
	int i, r;
	int64_t pos, prev = -1, stuck = 0;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, data, sizeof(data)));

	for (i = 0; i < 100; i++) {
		r = archive_read_next_header(a, &ae);
		if (r == ARCHIVE_EOF || r == ARCHIVE_FATAL)
			break;
		archive_read_data_skip(a);
		pos = archive_filter_bytes(a, -1);
		stuck = (pos == prev) ? stuck + 1 : 0;
		prev = pos;
		failure("archive_read_data_skip() must either make forward "
		    "progress or fail with ARCHIVE_FATAL; the reader stalled "
		    "at byte %d", (int)pos);
		assert(stuck < 3);
		if (stuck >= 3)
			break;
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
