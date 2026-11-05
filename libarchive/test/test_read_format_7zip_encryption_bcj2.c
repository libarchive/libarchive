/*-
 * Copyright (c) 2026 Witt Kung
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

static void
verify_bcj2(const char *ref, const unsigned char *data, size_t data_size,
    const unsigned char *tail, size_t tail_size, int header, size_t input_block,
    size_t output_block, int skip)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char buff[65536];
	char *archive_data;
	size_t i, total, size, request, archive_size;
	la_ssize_t n;
	int r;

	failure("%s, input %lu, output %lu, skip %d", ref,
	    (unsigned long)input_block, (unsigned long)output_block, skip);
	archive_data = slurpfile(&archive_size, "%s", ref);
	assert(archive_data != NULL);
	if (archive_data == NULL)
		return;
	assert((a = archive_read_new()) != NULL);
	if (a == NULL) {
		free(archive_data);
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_7zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_add_passphrase(a, "12345678"));
	assertEqualIntA(a, ARCHIVE_OK,
	    read_open_memory_seek(a, archive_data, archive_size, input_block));
	for (i = 0; i < 2; i++) {
		const unsigned char *expected = i == 0 ? data : tail;

		r = archive_read_next_header(a, &ae);
		assertEqualIntA(a, ARCHIVE_OK, r);
		if (r != ARCHIVE_OK)
			break;
		assertEqualString(i == 0 ? "data.bin" : "tail.txt",
		    archive_entry_pathname(ae));
		size = i == 0 ? data_size : tail_size;
		assertEqualInt(size, archive_entry_size(ae));
		assertEqualInt(1, archive_entry_is_data_encrypted(ae));
		assertEqualInt(header, archive_entry_is_metadata_encrypted(ae));
		if (i == 0 && skip) {
			if (skip == 2) {
				assertEqualIntA(a, 23, archive_read_data(a, buff, 23));
				assertEqualMem(expected, buff, 23);
				assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));
			}
			continue;
		}
		for (total = 0; total < size; total += (size_t)n) {
			request = output_block;
			if (request > size - total)
				request = size - total;
			n = archive_read_data(a, buff, request);
			failure("%s, entry %lu, input %lu, output %lu, skip %d",
			    ref, (unsigned long)i, (unsigned long)input_block,
			    (unsigned long)output_block, skip);
			assertEqualIntA(a, request, n);
			if (n <= 0 || (size_t)n > request)
				break;
			assertEqualMem(expected + total, buff, (size_t)n);
		}
		assertEqualInt(size, total);
		assertEqualIntA(a, 0, archive_read_data(a, buff, 1));
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(archive_data);
}

/* References generated and verified with 7-Zip, password "12345678".
 * Cover four independently encrypted inputs, six/nine coders, separately
 * compressed side streams, solid/non-solid files, empty branch streams,
 * and both unread and partially read file skipping.
 * The mixed case uses PPMd, BZip2, Deflate, and LZMA on the four inputs. */
DEFINE_TEST(test_read_format_7zip_encryption_bcj2)
{
	const char *refs[] = {
	    "test_read_format_7zip_encryption_bcj2.7z",
	    "test_read_format_7zip_encryption_bcj2_header.7z",
	    "test_read_format_7zip_encryption_bcj2_four.7z",
	    "test_read_format_7zip_encryption_bcj2_mixed.7z",
	    "test_read_format_7zip_encryption_bcj2_empty_streams.7z"
	};
	const int headers[] = {0, 1, 1, 0, 1};
	const size_t inputs[] = {1, 7, 15, 16, 17, 65536};
	const size_t outputs[] = {4093, 65536};
	static const char prefix[] = "BCJ2 ordinary archive test\n";
	static const char tail_line[] = "BCJ2 solid stream tail\n";
	static const char empty_line[] = "BCJ2 empty branch streams\n";
	unsigned char *data, *tail, *empty, *p;
	size_t i, j, k, l, size = 4096 * (sizeof(prefix) - 1 + 16);
	size_t tail_size = 4000 * (sizeof(tail_line) - 1);
	size_t empty_size = 4096 * (sizeof(empty_line) - 1);

	if (!can7zipEncryption() || archive_liblzma_version() == NULL) {
		skipping("7-Zip AES and LZMA decryption are required");
		return;
	}
	data = malloc(size);
	tail = malloc(tail_size);
	empty = malloc(empty_size);
	assert(data != NULL);
	assert(tail != NULL);
	assert(empty != NULL);
	if (data == NULL || tail == NULL || empty == NULL) {
		free(data);
		free(tail);
		free(empty);
		return;
	}
	p = data;
	for (i = 0; i < 4096; i++) {
		memcpy(p, prefix, sizeof(prefix) - 1);
		p += sizeof(prefix) - 1;
		*p++ = 0xe8;
		for (j = 0; j < 4; j++)
			*p++ = (unsigned char)(((i * 7) % 4096) >> (j * 8));
		*p++ = 0xe9;
		for (j = 0; j < 4; j++)
			*p++ = (unsigned char)(((i * 3) % 4096) >> (j * 8));
		*p++ = 0x0f;
		*p++ = 0x85;
		for (j = 0; j < 4; j++)
			*p++ = (unsigned char)((i % 2048) >> (j * 8));
		memcpy(empty + i * (sizeof(empty_line) - 1), empty_line,
		    sizeof(empty_line) - 1);
	}
	for (i = 0; i < 4000; i++)
		memcpy(tail + i * (sizeof(tail_line) - 1), tail_line,
		    sizeof(tail_line) - 1);
	for (i = 0; i < sizeof(refs) / sizeof(refs[0]); i++) {
		if (i == 3 && (archive_bzlib_version() == NULL ||
		    archive_zlib_version() == NULL)) {
			skipping("Mixed BCJ2 streams require BZip2 and Deflate");
			continue;
		}
		extract_reference_file(refs[i]);
		for (j = 0; j < sizeof(inputs) / sizeof(inputs[0]); j++) {
			for (k = 0; k < sizeof(outputs) / sizeof(outputs[0]); k++) {
				for (l = 0; l < 3; l++)
					verify_bcj2(refs[i], i == 4 ? empty : data,
					    i == 4 ? empty_size : size, tail, tail_size,
					    headers[i], inputs[j], outputs[k], (int)l);
			}
		}
	}
	free(data);
	free(tail);
	free(empty);
}
