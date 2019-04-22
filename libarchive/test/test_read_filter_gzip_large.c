/*-
 * Copyright (c) 2019 Ed Catmur
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

DEFINE_TEST(test_read_filter_gzip_large)
{
	char const header[10] = { 0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 4, 255 };
	char const block[5] = { 1, 0x34, 0x12, 0xcb, 0xed }; /* uncompressed */
	char const trailer[8] = { 0x89, 0xab, 0xcd, 0xef, 0x34, 0x12, 0, 0 }; /* bogus CRC32, we don't check */
	struct archive *a;
	struct archive_entry *ae;
	char* buf;
	unsigned i;
	int ret;
	void const* rbuf;
	size_t len;
	off_t offset;
	off_t total;
	int pattern;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_gzip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assert((buf = malloc(sizeof(header) + sizeof(block) + 0x1234u + sizeof(trailer))) != NULL);
	memcpy(buf, header, sizeof(header));
	memcpy(buf + sizeof(header), block, sizeof(block));
	memset(buf + sizeof(header) + sizeof(block), '\xaa', 0x1234u);
	memcpy(buf + sizeof(header) + sizeof(block) + 0x1234u, trailer, sizeof(trailer));
	/* Check that gzip filter can handle > 4 GiB available, since API is 32-bit */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buf, (1ull << 32) + sizeof(header) + 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	len = 0;
	offset = 0;
	total = 0;
	pattern = 0;
	for (;;) {
		ret = archive_read_data_block(a, &rbuf, &len, &offset);
		assertA(ret == ARCHIVE_OK || ret == ARCHIVE_EOF);
		if (ret != ARCHIVE_OK)
			break;
		assertEqualInt(offset, total);
		for (i = 0; i != len; ++i)
			pattern += ((char const*) rbuf)[i] == '\xaa';
		total += len;
		if (total > 0x1234)
			break;
	}
	assertEqualInt(total, 0x1234);
	assertEqualInt(pattern, 0x1234);

	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(buf);
}
