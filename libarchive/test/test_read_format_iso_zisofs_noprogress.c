/*-
 * Copyright (c) 2026
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
 * A zisofs block whose declared size is larger than its deflate stream
 * used to make the reader spin forever without consuming input.
 */
DEFINE_TEST(test_read_format_iso_zisofs_noprogress)
{
	static const unsigned char magic[] =
	    { 0x37, 0xE4, 0x53, 0x96, 0xC9, 0xDB, 0xD6, 0x07 };
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buff, *p;
	size_t buffsize = 2048 * 64, used, i, size = 40000;
	char *data;
	const void *rb;
	size_t rsize;
	int64_t roffset;
	int r, n;

	data = malloc(size);
	buff = malloc(buffsize);
	assert(data != NULL && buff != NULL);
	if (data == NULL || buff == NULL) {
		free(data);
		free(buff);
		return;
	}
	for (i = 0; i < size; i++)
		data[i] = "zisofs "[i % 7];

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, 0, archive_write_set_format_iso9660(a));
	assertEqualIntA(a, 0, archive_write_add_filter_none(a));
	if (archive_write_set_option(a, NULL, "zisofs", "1") == ARCHIVE_FATAL) {
		skipping("zisofs option not supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_write_free(a));
		free(data);
		free(buff);
		return;
	}
	assertEqualIntA(a, 0, archive_write_set_option(a, NULL, "pad", NULL));
	assertEqualIntA(a, 0,
	    archive_write_open_memory(a, buff, buffsize, &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0644);
	archive_entry_set_size(ae, size);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, size, archive_write_data(a, data, size));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * The file has two blocks.  Grow the end pointer of the first one,
	 * which is the second entry of the block pointer table after the
	 * 16 byte header.
	 */
	p = NULL;
	for (i = 0; i + sizeof(magic) + 28 <= used; i++) {
		if (memcmp(buff + i, magic, sizeof(magic)) == 0) {
			p = buff + i + 20;
			break;
		}
	}
	assert(p != NULL);
	if (p != NULL) {
		p[0] += 8;
		assert((a = archive_read_new()) != NULL);
		assertEqualIntA(a, 0, archive_read_support_format_all(a));
		assertEqualIntA(a, 0, archive_read_support_filter_all(a));
		assertEqualIntA(a, 0, archive_read_open_memory(a, buff, used));
		assertEqualIntA(a, 0, archive_read_next_header(a, &ae));
		assertEqualIntA(a, 0, archive_read_next_header(a, &ae));
		assertEqualString("file", archive_entry_pathname(ae));
		/* Bounded, so that a regression fails instead of hanging. */
		r = ARCHIVE_OK;
		for (n = 0; n < 100 && r == ARCHIVE_OK; n++)
			r = archive_read_data_block(a, &rb, &rsize, &roffset);
		assertEqualInt(ARCHIVE_FATAL, r);
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	}

	free(data);
	free(buff);
}
