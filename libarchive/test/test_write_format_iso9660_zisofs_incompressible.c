/*-SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Martin Mirchev
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
 * Write a zisofs file whose compressed data does not fit in the
 * writer's 64 KiB buffer, and read it back.  Incompressible data makes
 * this happen for a small file.
 *
 * The writer used to stop calling deflate() once a block's input was
 * consumed, even when the buffer had filled up and zlib still held the
 * end of the block, so that end was lost.  It also overwrote the
 * last partial logical block of the compressed data with the zisofs
 * header.
 */

#define ENTRY_SIZE	100000
#define WRITE_SIZE	10000

DEFINE_TEST(test_write_format_iso9660_zisofs_incompressible)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *data, *rbuff, *arcbuff;
	size_t buffsize = 1024 * 1024, used, i;
	uint32_t x = 2463534242U;
	int r;

	/* Pseudo-random data that zlib cannot compress (xorshift32). */
	data = malloc(ENTRY_SIZE);
	rbuff = malloc(ENTRY_SIZE);
	arcbuff = malloc(buffsize);
	assert(data != NULL && rbuff != NULL && arcbuff != NULL);
	if (data == NULL || rbuff == NULL || arcbuff == NULL) {
		free(data);
		free(rbuff);
		free(arcbuff);
		return;
	}
	for (i = 0; i < ENTRY_SIZE; i++) {
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		data[i] = (unsigned char)(x >> 24);
	}

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, 0, archive_write_set_format_iso9660(a));
	assertEqualIntA(a, 0, archive_write_add_filter_none(a));
	r = archive_write_set_option(a, NULL, "zisofs", "1");
	if (r == ARCHIVE_FATAL) {
		skipping("zisofs option not supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_write_free(a));
		free(data);
		free(rbuff);
		free(arcbuff);
		return;
	}
	assertEqualIntA(a, 0, archive_write_set_option(a, NULL, "pad", NULL));
	assertEqualIntA(a, 0, archive_write_open_memory(a, arcbuff, buffsize,
	    &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, ENTRY_SIZE);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	for (i = 0; i < ENTRY_SIZE; i += WRITE_SIZE)
		assertEqualIntA(a, WRITE_SIZE,
		    archive_write_data(a, data + i, WRITE_SIZE));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Read the file back and compare. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, 0, archive_read_support_format_all(a));
	assertEqualIntA(a, 0, archive_read_support_filter_all(a));
	assertEqualIntA(a, 0, archive_read_open_memory(a, arcbuff, used));

	/* The root directory. */
	assertEqualIntA(a, 0, archive_read_next_header(a, &ae));
	assertEqualString(".", archive_entry_pathname(ae));

	assertEqualIntA(a, 0, archive_read_next_header(a, &ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assertEqualInt(ENTRY_SIZE, archive_entry_size(ae));
	memset(rbuff, 0, ENTRY_SIZE);
	assertEqualIntA(a, ENTRY_SIZE,
	    archive_read_data(a, rbuff, ENTRY_SIZE));
	assertEqualMem(data, rbuff, ENTRY_SIZE);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(data);
	free(rbuff);
	free(arcbuff);
}
