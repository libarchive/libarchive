/*-
 * Copyright (c) 2026 Stefano Babic
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
 * The "070704" format is the SVR4 CRC format with a 64-bit file size,
 * used by SWUpdate for files of 4 GiB and more: c_rdevmajor has the
 * upper 32 bits of the size.  Only the entries of such files use it,
 * the other entries of the archive are "070702".
 */

/* Appends an entry with its data, padded as in the newc formats. */
static size_t
add_entry(char *buf, const char *magic, const char *name, uint64_t size,
    unsigned rdevmajor, const char *data)
{
	size_t namesize = strlen(name) + 1;
	size_t len, datalen = data ? strlen(data) : 0;

	len = (size_t)sprintf(buf,
	    "%s%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X",
	    magic, 1, 0100644, 0, 0, 1, 0, (unsigned)(size & 0xffffffff),
	    0, 0, rdevmajor, 0, (unsigned)namesize, 0);
	memcpy(buf + len, name, namesize);
	len += namesize;
	while (len % 4)
		buf[len++] = '\0';
	if (datalen) {
		memcpy(buf + len, data, datalen);
		len += datalen;
		while (len % 4)
			buf[len++] = '\0';
	}
	return (len);
}

static struct archive *
open_archive(const char *buf, size_t len)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cpio(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buf, len));
	return (a);
}

DEFINE_TEST(test_read_format_cpio_svr4_crc_large)
{
	static char buf[2048];
	struct archive_entry *ae;
	struct archive *a;
	char data[8];
	size_t len;

	/*
	 * A small file in the 64-bit format, read to the end: the
	 * archive starts with "070704".
	 */
	len = add_entry(buf, "070704", "small", 5, 0, "hello");
	len += add_entry(buf + len, "070702", "TRAILER!!!", 0, 0, NULL);
	a = open_archive(buf, len);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_FORMAT_CPIO_SVR4_CRC_LARGE, archive_format(a));
	assertEqualString("small", archive_entry_pathname(ae));
	assertEqualInt(5, archive_entry_size(ae));
	assertEqualInt(0, archive_entry_rdevmajor(ae));
	assertEqualInt(5, archive_read_data(a, data, sizeof(data)));
	assertEqualMem("hello", data, 5);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * A "070702" entry followed by a big one: the upper 32 bits of
	 * the size are in rdevmajor.  The data is not in the buffer,
	 * only the header is read.
	 */
	len = add_entry(buf, "070702", "sw-description", 4, 0, "desc");
	len += add_entry(buf + len, "070704", "rootfs", 0x123456789ULL, 1,
	    NULL);
	a = open_archive(buf, len);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_FORMAT_CPIO_SVR4_CRC, archive_format(a));
	assertEqualString("sw-description", archive_entry_pathname(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_FORMAT_CPIO_SVR4_CRC_LARGE, archive_format(a));
	assertEqualString("rootfs", archive_entry_pathname(ae));
	assertEqualInt(0x123456789LL, archive_entry_size(ae));
	assertEqualInt(0, archive_entry_rdevmajor(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* The search for the next header after junk finds "070704" too. */
	len = add_entry(buf, "070702", "a", 4, 0, "abcd");
	memset(buf + len, 'J', 3);
	len += 3;
	len += add_entry(buf + len, "070704", "b", 0x100000000ULL, 1, NULL);
	a = open_archive(buf, len);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("a", archive_entry_pathname(ae));
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	assertEqualString("b", archive_entry_pathname(ae));
	assertEqualInt(0x100000000LL, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* A size that does not fit into int64_t is refused. */
	len = add_entry(buf, "070704", "huge", 0, 0x80000000U, NULL);
	a = open_archive(buf, len);
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
