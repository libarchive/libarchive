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

/* The ordinary reference data is 70000 low bytes of xorshift32, seed 2.
 * 7-Zip PPMd with password "12345678"; the second archive also encrypts
 * its header. Both archives were independently verified with 7-Zip.
 * Decoding a symbol crosses the 64KiB decrypted-input boundary. */
DEFINE_TEST(test_read_format_7zip_encryption_ppmd)
{
	const char *refs[] = {
	    "test_read_format_7zip_encryption_ppmd.7z",
	    "test_read_format_7zip_encryption_ppmd_header.7z"
	};
	const size_t input_blocks[] = {7, 65536};
	const size_t output_blocks[] = {4093, 65536};
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *expected, *actual;
	uint32_t state = 2;
	size_t i, j, k, total, request;
	la_ssize_t n;

	if (!can7zipEncryption()) {
		skipping("7-Zip AES decryption is not supported on this platform");
		return;
	}
	expected = malloc(70000);
	actual = malloc(65536);
	assert(expected != NULL);
	assert(actual != NULL);
	if (expected == NULL || actual == NULL) {
		free(expected);
		free(actual);
		return;
	}
	for (i = 0; i < 70000; i++) {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		expected[i] = (unsigned char)state;
	}
	for (i = 0; i < sizeof(refs) / sizeof(refs[0]); i++) {
		extract_reference_file(refs[i]);
		for (j = 0; j < sizeof(input_blocks) / sizeof(input_blocks[0]); j++) {
			for (k = 0; k < sizeof(output_blocks) / sizeof(output_blocks[0]); k++) {
				assert((a = archive_read_new()) != NULL);
				assertEqualIntA(a, ARCHIVE_OK,
				    archive_read_support_format_7zip(a));
				assertEqualIntA(a, ARCHIVE_OK,
				    archive_read_add_passphrase(a, "12345678"));
				assertEqualIntA(a, ARCHIVE_OK,
				    archive_read_open_filename(a, refs[i], input_blocks[j]));
				assertEqualIntA(a, ARCHIVE_OK,
				    archive_read_next_header(a, &ae));
				assertEqualString("data.bin", archive_entry_pathname(ae));
				assertEqualInt(70000, archive_entry_size(ae));
				for (total = 0; total < 70000; total += (size_t)n) {
					request = output_blocks[k];
					if (request > 70000 - total)
						request = 70000 - total;
					n = archive_read_data(a, actual, request);
					assertEqualIntA(a, request, n);
					if (n <= 0 || (size_t)n > request)
						break;
					assertEqualMem(expected + total, actual, (size_t)n);
				}
				assertEqualInt(70000, total);
				assertEqualIntA(a, 0, archive_read_data(a, actual, 1));
				assertEqualIntA(a, ARCHIVE_EOF,
				    archive_read_next_header(a, &ae));
				assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
				assertEqualInt(ARCHIVE_OK, archive_read_free(a));
			}
		}
	}
	free(expected);
	free(actual);
}
