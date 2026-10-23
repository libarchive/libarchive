/*-
 * Copyright (c) 2026 libarchive contributors
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
 * archive_seek_data() moves in the data of an entry of a ZIP archive that is
 * stored: neither compressed nor encrypted. For any other entry, or for the
 * streaming reader, it fails and changes nothing.
 */

#define SIZE	100000
#define PASSWORD	"password1234"

/* A pattern that doesn't repeat every 256 bytes. */
static unsigned char
pattern(int64_t i)
{
	return ((unsigned char)((i * 7 + (i >> 8)) & 0xff));
}

/* Writes an archive with a big entry, an empty entry, a directory and a
 * small entry that comes last. */
static size_t
make_archive(char *buff, size_t buffsize, const char *options, int encrypt)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *data;
	size_t used = 0;
	int64_t i;

	assert((data = malloc(SIZE)) != NULL);
	for (i = 0; i < SIZE; i++)
		data[i] = pattern(i);

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, options));
	if (encrypt) {
		if (archive_write_set_options(a, "zip:encryption=zipcrypt")
		    != ARCHIVE_OK) {
			archive_write_free(a);
			free(data);
			return (0);
		}
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_write_set_passphrase(a, PASSWORD));
	}
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "big");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, SIZE);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(SIZE, archive_write_data(a, data, SIZE));
	archive_entry_free(ae);

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "empty");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "dir/");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "last");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 5);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(5, archive_write_data(a, "after", 5));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	free(data);
	return (used);
}

/* Reads n bytes and checks that they are the ones of the pattern from pos. */
static void
read_and_check(struct archive *a, int64_t pos, size_t n)
{
	unsigned char *buf;
	size_t i;

	assert((buf = malloc(n)) != NULL);
	failure("pos=%jd n=%zu", (intmax_t)pos, n);
	assertEqualInt((int)n, (int)archive_read_data(a, buf, n));
	for (i = 0; i < n; i++) {
		if (buf[i] != pattern(pos + (int64_t)i)) {
			failure("pos=%jd i=%zu", (intmax_t)pos, i);
			assertEqualInt(pattern(pos + (int64_t)i), buf[i]);
			break;
		}
	}
	free(buf);
}

static struct archive *
open_archive(const char *buff, size_t used, int streaming, int encrypt)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	if (streaming)
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_zip_streamable(a));
	else
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_zip_seekable(a));
	if (encrypt)
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_add_passphrase(a, PASSWORD));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	return (a);
}

DEFINE_TEST(test_read_format_zip_seek_data)
{
	static char buff[SIZE + 10000];
	struct archive *a;
	struct archive_entry *ae;
	const void *block;
	size_t used, block_size;
	int64_t block_offset;
	char tail[16];

	used = make_archive(buff, sizeof(buff), "zip:compression=store", 0);
	assert(used > SIZE);

	/* Moving around in a stored entry. */
	a = open_archive(buff, used, 0, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("big", archive_entry_pathname(ae));

	assertEqualInt(1000, archive_seek_data(a, 1000, SEEK_SET));
	read_and_check(a, 1000, 200);
	/* The block that was returned for the 200 bytes is probably bigger,
	 * but the current position is where the caller is. */
	assertEqualInt(1200, archive_seek_data(a, 0, SEEK_CUR));
	assertEqualInt(1500, archive_seek_data(a, 300, SEEK_CUR));
	read_and_check(a, 1500, 100);
	assertEqualInt(600, archive_seek_data(a, -1000, SEEK_CUR));
	read_and_check(a, 600, 10);
	assertEqualInt(SIZE - 50, archive_seek_data(a, -50, SEEK_END));
	read_and_check(a, SIZE - 50, 50);
	/* At the end. */
	assertEqualInt(0, archive_read_data(a, tail, sizeof(tail)));
	assertEqualInt(SIZE, archive_seek_data(a, 0, SEEK_END));
	assertEqualInt(0, archive_read_data(a, tail, sizeof(tail)));

	/* From the end, back to the start, and the whole entry is read and
	 * checked. */
	assertEqualInt(0, archive_seek_data(a, 0, SEEK_SET));
	read_and_check(a, 0, SIZE);
	assertEqualInt(0, archive_read_data(a, tail, sizeof(tail)));

	/* A position that is not valid changes nothing. */
	assertEqualInt(100, archive_seek_data(a, 100, SEEK_SET));
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, -1, SEEK_SET));
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, -101, SEEK_CUR));
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, -SIZE - 1, SEEK_END));
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, 0, 12345));
	assertEqualInt(100, archive_seek_data(a, 0, SEEK_CUR));
	read_and_check(a, 100, 20);

	/* After the end of the data, there is nothing to read. The position
	 * is where it was asked to be. */
	assertEqualInt(SIZE + 1, archive_seek_data(a, SIZE + 1, SEEK_SET));
	assertEqualInt(0, archive_read_data(a, tail, sizeof(tail)));
	assertEqualInt(SIZE + 1, archive_seek_data(a, 0, SEEK_CUR));
	assertEqualInt(ARCHIVE_EOF,
	    archive_read_data_block(a, &block, &block_size, &block_offset));
	assertEqualInt(0, block_size);
	assertEqualInt(SIZE, block_offset);
	assertEqualInt(SIZE + 1000, archive_seek_data(a, 1000, SEEK_END));
	assertEqualInt(SIZE + 1000, archive_seek_data(a, 0, SEEK_CUR));
	assertEqualInt(SIZE + 2000, archive_seek_data(a, 1000, SEEK_CUR));
	/* And back to the data. */
	assertEqualInt(SIZE - 100, archive_seek_data(a, -2100, SEEK_CUR));
	read_and_check(a, SIZE - 100, 100);
	assertEqualInt(0, archive_read_data(a, tail, sizeof(tail)));
	assertEqualInt(SIZE + 5, archive_seek_data(a, 5, SEEK_END));
	assertEqualInt(100, archive_seek_data(a, 100, SEEK_SET));
	read_and_check(a, 100, 20);

	/* The blocks have the real offset. */
	assertEqualInt(5000, archive_seek_data(a, 5000, SEEK_SET));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_data_block(a, &block, &block_size, &block_offset));
	assertEqualInt(5000, block_offset);
	assert(block_size > 0);
	assertEqualInt(pattern(5000), ((const unsigned char *)block)[0]);

	/* Moving does not harm the next entries. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("empty", archive_entry_pathname(ae));
	assertEqualInt(0, archive_seek_data(a, 0, SEEK_SET));
	assertEqualInt(0, archive_seek_data(a, 0, SEEK_END));
	assertEqualInt(1, archive_seek_data(a, 1, SEEK_SET));
	assertEqualInt(0, archive_read_data(a, tail, sizeof(tail)));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dir/", archive_entry_pathname(ae));
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, 0, SEEK_SET));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("last", archive_entry_pathname(ae));
	assertEqualInt(2, archive_seek_data(a, 2, SEEK_SET));
	assertEqualInt(3, archive_read_data(a, tail, sizeof(tail)));
	assertEqualMem("ter", tail, 3);
	assertEqualInt(ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * The checksum is verified if the whole entry is read, and not if the
 * reader moved to the middle of the entry.
 */
DEFINE_TEST(test_read_format_zip_seek_data_crc)
{
	static char buff[SIZE + 10000];
	static unsigned char out[SIZE + 16];
	struct archive *a;
	struct archive_entry *ae;
	unsigned char needle[16];
	size_t used, i, j;
	int found = 0;

	used = make_archive(buff, sizeof(buff), "zip:compression=store", 0);
	assert(used > SIZE);

	/* Damage a byte of the data of the entry, in the second half. */
	for (i = 0; i < sizeof(needle); i++)
		needle[i] = pattern(60000 + (int64_t)i);
	for (j = 0; j + sizeof(needle) < used; j++) {
		if (memcmp(buff + j, needle, sizeof(needle)) == 0) {
			buff[j + 3] ^= 0x55;
			found = 1;
			break;
		}
	}
	assert(found);

	/* Reading it all, from the start, fails at the end. */
	a = open_archive(buff, used, 0, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(SIZE, archive_read_data(a, out, SIZE));
	assertEqualInt(ARCHIVE_FAILED, archive_read_data(a, out, 10));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Asking for the position does not prevent the verification. */
	a = open_archive(buff, used, 0, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(SIZE / 2, archive_read_data(a, out, SIZE / 2));
	assertEqualInt(SIZE / 2, archive_seek_data(a, 0, SEEK_CUR));
	assertEqualInt(SIZE / 2, archive_read_data(a, out, SIZE / 2));
	assertEqualInt(SIZE, archive_seek_data(a, 0, SEEK_CUR));
	assertEqualInt(ARCHIVE_FAILED, archive_read_data(a, out, 10));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* After a move to a place after the damage, the checksum is not
	 * verified. */
	a = open_archive(buff, used, 0, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(70000, archive_seek_data(a, 70000, SEEK_SET));
	assertEqualInt(SIZE - 70000, archive_read_data(a, out, SIZE));
	assertEqualInt(0, archive_read_data(a, out, 10));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Going past the end, then back to the start, verifies it again. */
	a = open_archive(buff, used, 0, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(SIZE + 70000, archive_seek_data(a, SIZE + 70000, SEEK_SET));
	assertEqualInt(0, archive_read_data(a, out, 10));
	assertEqualInt(0, archive_seek_data(a, 0, SEEK_SET));
	assertEqualInt(SIZE, archive_read_data(a, out, SIZE));
	assertEqualInt(ARCHIVE_FAILED, archive_read_data(a, out, 10));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* An entry that is compressed cannot be seeked. Reading it still works. */
DEFINE_TEST(test_read_format_zip_seek_data_deflate)
{
	static char buff[SIZE + 10000];
	static unsigned char data[SIZE];
	struct archive *a;
	struct archive_entry *ae;
	size_t used;
	int64_t i;

	if (archive_zlib_version() == NULL) {
		skipping("zlib is not available");
		return;
	}
	used = make_archive(buff, sizeof(buff), "zip:compression=deflate", 0);
	assert(used > 0);

	a = open_archive(buff, used, 0, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("big", archive_entry_pathname(ae));
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, 1000, SEEK_SET));
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, 0, SEEK_CUR));
	/* The failure left the position as it was. */
	read_and_check(a, 0, 3000);
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, 0, SEEK_SET));
	assertEqualInt(SIZE - 3000, archive_read_data(a, data, SIZE - 3000));
	for (i = 0; i < SIZE - 3000; i++)
		if (data[i] != pattern(3000 + i)) {
			failure("i=%jd", (intmax_t)i);
			assertEqualInt(pattern(3000 + i), data[i]);
			break;
		}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* An entry that is encrypted cannot be seeked, even if it is stored. */
DEFINE_TEST(test_read_format_zip_seek_data_encrypted)
{
	static char buff[SIZE + 10000];
	struct archive *a;
	struct archive_entry *ae;
	size_t used;

	used = make_archive(buff, sizeof(buff), "zip:compression=store", 1);
	if (used == 0) {
		skipping("This system does not have cryptographic library");
		return;
	}

	a = open_archive(buff, used, 0, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("big", archive_entry_pathname(ae));
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, 1000, SEEK_SET));
	read_and_check(a, 0, 3000);
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, 0, SEEK_SET));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* The streaming reader never sees the Central Directory. */
DEFINE_TEST(test_read_format_zip_seek_data_streaming)
{
	static char buff[SIZE + 10000];
	struct archive *a;
	struct archive_entry *ae;
	size_t used;

	used = make_archive(buff, sizeof(buff), "zip:compression=store", 0);
	assert(used > SIZE);

	a = open_archive(buff, used, 1, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("big", archive_entry_pathname(ae));
	assertEqualInt(ARCHIVE_FAILED, archive_seek_data(a, 1000, SEEK_SET));
	read_and_check(a, 0, 3000);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
