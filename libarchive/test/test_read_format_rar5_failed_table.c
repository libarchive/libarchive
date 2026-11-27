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
 * A decompression failure leaves the RAR5 decoder partway through a
 * block.  Resuming it must never be possible: with a rejected
 * (over-subscribed) Huffman table, the decoder used to emit bytes
 * without consuming input, spinning until it reached the entry's
 * declared size.
 *
 * test_read_format_rar5_failed_table_solid.rar was supplied by
 * Dickson Wu (SEAL & CertiK).  It is a solid archive (main archive
 * flags 0x04) holding one compressed entry, "test.bin", which declares
 * an unpacked size of 1 TiB and whose compressed data contains an
 * over-subscribed literal table.
 *
 * test_read_format_rar5_failed_table_nonsolid.rar is the same file
 * with the main archive flags byte (offset 16) changed from 0x04 to
 * 0x00 and the main header CRC32 (offset 8) recomputed.
 */

/* Solid, listing only: skipping the entry decodes it, and the failure
 * must be fatal rather than letting the next skip resume the block. */
DEFINE_TEST(test_read_format_rar5_failed_table_solid_skip)
{
	const char *reffile = "test_read_format_rar5_failed_table_solid.rar";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualString("Failed to create literal table",
	    archive_error_string(a));
	/* Sticky. */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* Solid, reading the data: the failure must be fatal, so the
 * following header read cannot resume the block by skipping. */
DEFINE_TEST(test_read_format_rar5_failed_table_solid_read)
{
	const char *reffile = "test_read_format_rar5_failed_table_solid.rar";
	struct archive_entry *ae;
	struct archive *a;
	const void *buf;
	size_t size;
	la_int64_t offset;
	int r;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	while ((r = archive_read_data_block(a, &buf, &size, &offset))
	    == ARCHIVE_OK)
		continue;
	assertEqualIntA(a, ARCHIVE_FATAL, r);
	assertEqualString("Failed to create literal table",
	    archive_error_string(a));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* Non-solid: only this entry is lost.  Retrying the read must not
 * resume the decoder, and the rest of the archive stays readable. */
DEFINE_TEST(test_read_format_rar5_failed_table_nonsolid)
{
	const char *reffile = "test_read_format_rar5_failed_table_nonsolid.rar";
	struct archive_entry *ae;
	struct archive *a;
	const void *buf;
	size_t size;
	la_int64_t offset;
	int r;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	while ((r = archive_read_data_block(a, &buf, &size, &offset))
	    == ARCHIVE_OK)
		continue;
	assertEqualIntA(a, ARCHIVE_FAILED, r);
	assertEqualString("Failed to create literal table",
	    archive_error_string(a));

	/* A retry must fail again, without producing any data. */
	size = 1;
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_read_data_block(a, &buf, &size, &offset));
	assertEqualInt(0, size);

	/* Skipping the damaged entry reaches the end of the archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
