/*-
 * Copyright (c) 2026 Mayank Jangid
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

enum {
	LINK_CRC_OFFSET = 14,
	LINK_COMPRESSED_SIZE_OFFSET = 18,
	LINK_UNCOMPRESSED_SIZE_OFFSET = 22,
	LINK_DATA_OFFSET = 43,
	DESCRIPTOR_CRC_OFFSET = 60,
	DEFLATE_LINK_COMPRESSED_SIZE = 13,
	LZMA_LINK_COMPRESSED_SIZE = 31
};

static void
set_le32(char *p, unsigned long value)
{
	p[0] = (char)(value & 0xff);
	p[1] = (char)((value >> 8) & 0xff);
	p[2] = (char)((value >> 16) & 0xff);
	p[3] = (char)((value >> 24) & 0xff);
}

static void
verify_archive(const void *p, size_t s, size_t read_size, int seekable)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[901];
	char expected[900];
	int i;
	int r;
	ssize_t n;

	for (i = 0; i < 50; i++)
		memcpy(expected + i * 18, "TAIL-AFTER-SYMLINK", 18);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	if (seekable) {
		assertEqualIntA(a, ARCHIVE_OK,
		    read_open_memory_seek(a, p, s, read_size));
	} else {
		assertEqualIntA(a, ARCHIVE_OK,
		    read_open_memory_minimal(a, p, s, read_size));
	}

	failure("Cannot read compressed symlink with %s reader and "
	    "%zu-byte blocks", seekable ? "seekable" : "streaming",
	    read_size);
	r = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, r);
	if (r != ARCHIVE_OK) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}
	assertEqualString("link", archive_entry_pathname(ae));
	assertEqualInt(AE_IFLNK, archive_entry_filetype(ae));
	assertEqualString("safe-target", archive_entry_symlink(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));

	r = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, r);
	if (r != ARCHIVE_OK) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}
	assertEqualString("file", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(900, archive_entry_size(ae));
	n = archive_read_data(a, buff, sizeof(buff));
	assertEqualInt(900, n);
	if (n == 900)
		assertEqualMem(expected, buff, sizeof(expected));
	assertEqualInt(0, archive_read_data(a, buff, sizeof(buff)));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
verify_rejected(const void *p, size_t s, size_t field_offset,
	unsigned long value, int expected_result, int expected_errno,
	const char *error_fragment, int can_continue)
{
	struct archive *a;
	struct archive_entry *ae;
	char *copy;
	int r;

	assert(field_offset + 4 <= s);
	assert((copy = malloc(s)) != NULL);
	memcpy(copy, p, s);
	set_le32(copy + field_offset, value);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    read_open_memory_minimal(a, copy, s, 7));
	failure("Malformed compressed symlink field at offset %zu has value %lu",
	    field_offset, value);
	r = archive_read_next_header(a, &ae);
	assertEqualIntA(a, expected_result, r);
	if (r != expected_result) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		free(copy);
		return;
	}
	if (expected_errno == 0)
		assert(archive_errno(a) != 0);
	else
		assertEqualInt(expected_errno, archive_errno(a));
	if (error_fragment != NULL)
		assert(strstr(archive_error_string(a), error_fragment) != NULL);
	if (can_continue) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_next_header(a, &ae));
		assertEqualString("file", archive_entry_pathname(ae));
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(copy);
}

static void
verify_corrupt_deflate(const void *p, size_t s)
{
	struct archive *a;
	struct archive_entry *ae;
	char *copy;

	assert(LINK_DATA_OFFSET + 2 < s);
	assert((copy = malloc(s)) != NULL);
	memcpy(copy, p, s);
	copy[LINK_DATA_OFFSET] ^= 0xff;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    read_open_memory_minimal(a, copy, s, 7));
	failure("Deflate symlink body is corrupt");
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assert(archive_errno(a) != 0);
	assert(strstr(archive_error_string(a),
	    "ZIP decompression failed") != NULL);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(copy);
}

static void
verify_truncated(const void *p, size_t s, size_t compressed_size)
{
	struct archive *a;
	struct archive_entry *ae;

	assert(LINK_DATA_OFFSET + compressed_size - 1 <= s);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_minimal(a, p,
	    LINK_DATA_OFFSET + compressed_size - 1, 7));
	failure("Compressed symlink body is truncated to %zu bytes",
	    compressed_size - 1);
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assert(archive_errno(a) != 0);
	assert(strstr(archive_error_string(a),
	    "Truncated ZIP symlink body") != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
verify_oversized_declared_length_at_end(const void *p, size_t s)
{
	struct archive *a;
	struct archive_entry *ae;
	char *copy;

	assert(LINK_COMPRESSED_SIZE_OFFSET + 4 <= s);
	assert((copy = malloc(s)) != NULL);
	memcpy(copy, p, s);
	copy[6] = 8;
	copy[7] = 0;
	set_le32(copy + LINK_COMPRESSED_SIZE_OFFSET, 64 * 1024 + 1);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    read_open_memory_minimal(a, copy, s, 7));
	failure("Length-at-end symlink has an oversized declared body");
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualInt(-1, archive_errno(a));
	assert(strstr(archive_error_string(a),
	    "oversized link entry") != NULL);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(copy);
}

static void
verify_oversized_stream(const void *p, size_t s)
{
	struct archive *a;
	struct archive_entry *ae;
	char *copy;
	char *q;
	size_t empty_blocks = 64 * 1024 / 5 + 1;
	size_t i;
	size_t oversized_size = LINK_DATA_OFFSET + empty_blocks * 5 +
	    DEFLATE_LINK_COMPRESSED_SIZE;

	assert(LINK_DATA_OFFSET + DEFLATE_LINK_COMPRESSED_SIZE <= s);
	assert((copy = malloc(oversized_size)) != NULL);
	memcpy(copy, p, LINK_DATA_OFFSET);
	copy[6] = 8;
	copy[7] = 0;
	memset(copy + LINK_CRC_OFFSET, 0, 12);
	q = copy + LINK_DATA_OFFSET;
	for (i = 0; i < empty_blocks; i++) {
		*q++ = 0;
		*q++ = 0;
		*q++ = 0;
		*q++ = (char)0xff;
		*q++ = (char)0xff;
	}
	memcpy(q, (const char *)p + LINK_DATA_OFFSET,
	    DEFLATE_LINK_COMPRESSED_SIZE);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_minimal(a, copy,
	    oversized_size, oversized_size));
	failure("Length-at-end symlink has more than 64 KiB of valid "
	    "deflate input");
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assert(archive_errno(a) != 0);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(copy);
}

static void
test_deflate(void)
{
	const char *refname = "test_read_format_zip_symlink_deflate.zip";
	const char *descriptor_refname =
	    "test_read_format_zip_symlink_deflate_descriptor.zip";
	char *descriptor;
	char *p;
	size_t descriptor_size;
	size_t s;

	if (archive_zlib_version() == NULL) {
		return;
	}

	extract_reference_file(refname);
	p = slurpfile(&s, "%s", refname);
	assert(p != NULL);

	/* Exercise streaming with partial and complete input blocks. */
	verify_archive(p, s, 1, 0);
	verify_archive(p, s, 7, 0);
	verify_archive(p, s, s, 0);
	verify_archive(p, s, 7, 1);

	/* Compressed symlinks must receive ordinary entry validation. */
	verify_rejected(p, s, LINK_CRC_OFFSET, 0xdcd76aa3,
	    ARCHIVE_FAILED, -1, "ZIP bad CRC", 1);
	verify_rejected(p, s, LINK_UNCOMPRESSED_SIZE_OFFSET, 10,
	    ARCHIVE_FAILED, -1, "ZIP uncompressed data is wrong size", 1);
	verify_rejected(p, s, LINK_UNCOMPRESSED_SIZE_OFFSET, 12,
	    ARCHIVE_FAILED, -1, NULL, 1);
	verify_rejected(p, s, LINK_COMPRESSED_SIZE_OFFSET, 12,
	    ARCHIVE_FATAL, -1, "ZIP decompression failed", 0);
	verify_rejected(p, s, LINK_COMPRESSED_SIZE_OFFSET, 14,
	    ARCHIVE_FAILED, 0, "Invalid compressed ZIP symlink body", 1);
	verify_corrupt_deflate(p, s);
	verify_truncated(p, s, DEFLATE_LINK_COMPRESSED_SIZE);
	verify_oversized_declared_length_at_end(p, s);
	verify_oversized_stream(p, s);

	free(p);

	extract_reference_file(descriptor_refname);
	descriptor = slurpfile(&descriptor_size, "%s", descriptor_refname);
	assert(descriptor != NULL);

	verify_archive(descriptor, descriptor_size, 1, 0);
	verify_archive(descriptor, descriptor_size, 7, 0);
	verify_archive(descriptor, descriptor_size, descriptor_size, 0);
	verify_archive(descriptor, descriptor_size, 7, 1);
	verify_rejected(descriptor, descriptor_size, DESCRIPTOR_CRC_OFFSET,
	    0xdcd76aa3, ARCHIVE_FAILED, -1, "ZIP bad CRC", 1);

	free(descriptor);
}

static void
test_lzma(void)
{
	const char *refnames[] = {
		"test_read_format_zip_symlink_lzma.zip",
		"test_read_format_zip_symlink_lzma_no_eos.zip"
	};
	char *p;
	size_t i;
	size_t s;

	if (archive_liblzma_version() == NULL) {
		return;
	}

	for (i = 0; i < sizeof(refnames) / sizeof(refnames[0]); i++) {
		extract_reference_file(refnames[i]);
		p = slurpfile(&s, "%s", refnames[i]);
		assert(p != NULL);

		verify_archive(p, s, 1, 0);
		verify_archive(p, s, 7, 0);
		verify_archive(p, s, s, 0);
		verify_archive(p, s, 7, 1);

		if (i == 0) {
			verify_rejected(p, s, LINK_COMPRESSED_SIZE_OFFSET,
			    LZMA_LINK_COMPRESSED_SIZE + 1, ARCHIVE_FATAL,
			    -1, "lzma alone premature end of stream", 0);
			verify_truncated(p, s, LZMA_LINK_COMPRESSED_SIZE);
		}

		free(p);
	}
}

DEFINE_TEST(test_read_format_zip_symlink_compressed)
{
	if (archive_zlib_version() == NULL &&
	    archive_liblzma_version() == NULL) {
		skipping("deflate and LZMA are not supported on this platform");
		return;
	}
	test_deflate();
	test_lzma();
}
