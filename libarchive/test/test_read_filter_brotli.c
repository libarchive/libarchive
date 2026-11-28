/*-
 * Copyright (c) 2026 François Degros
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
 * A Brotli stream has no signature, so the filter is never chosen
 * automatically: these tests add it explicitly.
 */

/* "Hello, Brotli! " repeated three times, with a newline, made by brotli. */
static const char hello_txt[] = "Hello, Brotli! Hello, Brotli! Hello, Brotli!\n";
static const unsigned char hello_br[] = {
	0xa1, 0x60, 0x01, 0xc0, 0xef, 0x4c, 0x1e, 0xf7, 0xde, 0x69, 0xd4, 0xd4,
	0x4e, 0x45, 0xcd, 0xf6, 0x86, 0x94, 0x36, 0x41, 0x14, 0x84, 0xaa, 0x4c,
	0x2e, 0x43, 0x58, 0xe8, 0x4a, 0x11, 0xa6, 0x42, 0x80, 0x7d, 0x28, 0x6f,
	0x47
};

/*
 * The 129-byte line "0123456789abcdef" * 8 + "\n", repeated 1500 times:
 * 193500 bytes, much more than the filter's output buffer.
 */
#define BIG_LINES	1500
#define BIG_LINE_SIZE	129
static const unsigned char big_br[] = {
	0x53, 0xdb, 0xf3, 0x82, 0x5f, 0x16, 0x8f, 0xf9, 0x8c, 0xa2, 0x0d, 0xeb,
	0xaf, 0xc7, 0x12, 0xac, 0x01, 0x10, 0xa9, 0x42, 0x62, 0x51, 0xf3, 0xc8,
	0xea, 0xd9, 0x7b, 0x9f, 0x8a, 0x75, 0x01, 0x18
};

/*
 * Does this system have the Brotli decoder? It is either built in, or
 * provided by the "brotli" program.
 */
static int
has_brotli(void)
{
	struct archive *a;
	int r;

	assert((a = archive_read_new()) != NULL);
	r = archive_read_support_filter_brotli(a);
	archive_read_free(a);
	return (r == ARCHIVE_OK || (r == ARCHIVE_WARN && canBrotli()));
}

/*
 * Is the decoder built in the library? The program doesn't report errors
 * the same way.
 */
static int
has_builtin_brotli(void)
{
	struct archive *a;
	int r;

	assert((a = archive_read_new()) != NULL);
	r = archive_read_support_filter_brotli(a);
	archive_read_free(a);
	return (r == ARCHIVE_OK);
}

/*
 * Opens an in-memory stream with the Brotli filter and the raw format, which
 * returns the decompressed data as a single file.
 */
static struct archive *
open_brotli(const void *data, size_t size, size_t block_size)
{
	struct archive *a;
	int r;

	assert((a = archive_read_new()) != NULL);
	/* ARCHIVE_WARN means that it runs the "brotli" program. */
	r = archive_read_append_filter(a, ARCHIVE_FILTER_BROTLI);
	assert(r == ARCHIVE_OK || r == ARCHIVE_WARN);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory2(a, data, size, block_size));
	return (a);
}

DEFINE_TEST(test_read_filter_brotli)
{
	struct archive_entry *ae;
	struct archive *a;
	char buf[128];
	size_t block_size;

	if (!has_brotli()) {
		skipping("Brotli is not supported on this platform");
		return;
	}

	/* Try the whole input at once, and then one byte at a time. */
	for (block_size = sizeof(hello_br); block_size > 0;
	    block_size = block_size > 1 ? 1 : 0) {
		a = open_brotli(hello_br, sizeof(hello_br), block_size);
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualInt(ARCHIVE_FILTER_BROTLI, archive_filter_code(a, 0));
		assertEqualString("brotli", archive_filter_name(a, 0));
		assertEqualIntA(a, (int)(sizeof(hello_txt) - 1),
		    archive_read_data(a, buf, sizeof(buf)));
		assertEqualMem(hello_txt, buf, sizeof(hello_txt) - 1);
		assertEqualIntA(a, 0, archive_read_data(a, buf, sizeof(buf)));
		assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	}
}

/* The output is bigger than the buffer of the filter. */
DEFINE_TEST(test_read_filter_brotli_large)
{
	struct archive_entry *ae;
	struct archive *a;
	char line[BIG_LINE_SIZE];
	char buf[1000];
	size_t total = 0;
	ssize_t n;
	int i;

	if (!has_brotli()) {
		skipping("Brotli is not supported on this platform");
		return;
	}

	for (i = 0; i < 8; i++)
		memcpy(line + 16 * i, "0123456789abcdef", 16);
	line[BIG_LINE_SIZE - 1] = '\n';

	a = open_brotli(big_br, sizeof(big_br), sizeof(big_br));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	while ((n = archive_read_data(a, buf, sizeof(buf))) > 0) {
		/* Check the data line by line, wherever the reads end. */
		ssize_t j;

		for (j = 0; j < n; j++) {
			if (buf[j] != line[(total + j) % BIG_LINE_SIZE]) {
				failure("wrong byte at offset %zu", total + j);
				assert(0);
				break;
			}
		}
		total += n;
	}
	assertEqualInt(0, (int)n);
	assertEqualInt((int)(BIG_LINES * BIG_LINE_SIZE), (int)total);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* Brotli can't be detected, so the filter doesn't bid on anything. */
DEFINE_TEST(test_read_filter_brotli_not_detected)
{
	struct archive_entry *ae;
	struct archive *a;
	char buf[128];
	int r;

	if (!has_brotli()) {
		skipping("Brotli is not supported on this platform");
		return;
	}

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	/* ARCHIVE_WARN means that it would run the "brotli" program. */
	r = archive_read_support_filter_brotli(a);
	assert(r == ARCHIVE_OK || r == ARCHIVE_WARN);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, hello_br, sizeof(hello_br)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	/* The data comes out as it is. */
	assertEqualInt(ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, (int)sizeof(hello_br),
	    archive_read_data(a, buf, sizeof(buf)));
	assertEqualMem(hello_br, buf, sizeof(hello_br));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Reads the data block by block until the end, or until an error, and puts
 * the data in buf. Returns the status that ends the reading. Unlike
 * archive_read_data(), this keeps the data that was read before an error.
 */
static int
read_all(struct archive *a, char *buf, size_t size, size_t *total)
{
	const void *block;
	size_t block_size;
	int64_t offset;
	int r;

	*total = 0;
	while ((r = archive_read_data_block(a, &block, &block_size,
	    &offset)) == ARCHIVE_OK) {
		assert(*total + block_size <= size);
		memcpy(buf + *total, block, block_size);
		*total += block_size;
	}
	return (r);
}

/* Truncated, corrupt, empty and trailing data are errors. */
DEFINE_TEST(test_read_filter_brotli_errors)
{
	struct archive_entry *ae;
	struct archive *a;
	char buf[128];
	size_t total;
	unsigned char data[sizeof(hello_br) + 1];

	if (!has_builtin_brotli()) {
		skipping("The Brotli decoder is not built in");
		return;
	}

	/*
	 * Truncated: the decompressor has already produced some data from
	 * the first bytes. That data is returned before the error.
	 */
	a = open_brotli(hello_br, sizeof(hello_br) - 1, sizeof(hello_br));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_FATAL, read_all(a, buf, sizeof(buf), &total));
	assert(total > 0);
	assert(total < sizeof(hello_txt) - 1);
	assertEqualMem(hello_txt, buf, total);
	assertEqualString("Truncated Brotli input", archive_error_string(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * Trailing data: the stream is decoded, then the data is refused.
	 */
	memcpy(data, hello_br, sizeof(hello_br));
	data[sizeof(hello_br)] = 'x';
	a = open_brotli(data, sizeof(data), sizeof(data));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_FATAL, read_all(a, buf, sizeof(buf), &total));
	assertEqualInt((int)(sizeof(hello_txt) - 1), (int)total);
	assertEqualMem(hello_txt, buf, sizeof(hello_txt) - 1);
	assertEqualString("Unexpected data after the end of the Brotli stream",
	    archive_error_string(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * Corrupt and empty input fail before any data can be decompressed.
	 * The raw format reads the first bytes to open the archive, so that is
	 * where it fails.
	 */
	memset(data, 0xff, sizeof(data));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_append_filter(a, ARCHIVE_FILTER_BROTLI));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_open_memory(a, data, sizeof(data)));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_append_filter(a, ARCHIVE_FILTER_BROTLI));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_open_memory(a, hello_br, 0));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
