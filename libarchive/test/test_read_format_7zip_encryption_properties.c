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

#define __LIBARCHIVE_BUILD 1
#include "archive_cryptor_private.h"
#include "archive_digest_private.h"

int
can7zipEncryption(void)
{
#if defined(ARCHIVE_HAS_SHA256) && \
    (defined(ARCHIVE_CRYPTOR_USE_Apple_CommonCrypto) || \
     defined(ARCHIVE_CRYPTOR_USE_CNG) || defined(ARCHIVE_CRYPTOR_USE_MBED) || \
     defined(ARCHIVE_CRYPTOR_USE_NETTLE) || defined(ARCHIVE_CRYPTOR_USE_OPENSSL))
	return (1);
#else
	return (0);
#endif
}

/* Minimal AES-only archives, verified with 7-Zip. Password: "12345678".
 * Cover a single SHA-256 round, the direct-key mode, and salt/IV lengths
 * whose high flag is zero while the corresponding size nibble is nonzero.
 * Plaintext: "7zip encryption edge case\n". Salt: "salt".
 * IV bytes: 0..15 for salt_size, 0..11 for iv_size; otherwise no salt/IV.
 */
DEFINE_TEST(test_read_format_7zip_encryption_properties)
{
	const char *refs[] = {
	    "test_read_format_7zip_encryption_cycles0.7z",
	    "test_read_format_7zip_encryption_cycles63.7z",
	    "test_read_format_7zip_encryption_salt_size.7z",
	    "test_read_format_7zip_encryption_iv_size.7z"
	};
	const size_t blocks[] = {1, 7, 16, 10240};
	static const char expected[] = "7zip encryption edge case\n";
	struct archive *a;
	struct archive_entry *ae;
	char buff[64];
	size_t i, j;

	if (!can7zipEncryption()) {
		skipping("7-Zip AES decryption is not supported on this platform");
		return;
	}
	for (i = 0; i < sizeof(refs) / sizeof(refs[0]); i++) {
		extract_reference_file(refs[i]);
		for (j = 0; j < sizeof(blocks) / sizeof(blocks[0]); j++) {
			assert((a = archive_read_new()) != NULL);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_format_7zip(a));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_add_passphrase(a, "12345678"));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_open_filename(a, refs[i], blocks[j]));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_next_header(a, &ae));
			assertEqualString("edge.txt", archive_entry_pathname(ae));
			assertEqualInt(sizeof(expected) - 1, archive_entry_size(ae));
			assertEqualIntA(a, sizeof(expected) - 1,
			    archive_read_data(a, buff, sizeof(buff)));
			assertEqualMem(expected, buff, sizeof(expected) - 1);
			assertEqualInt(1, archive_entry_is_data_encrypted(ae));
			assertEqualInt(0, archive_entry_is_metadata_encrypted(ae));
			assertEqualIntA(a, 0, archive_read_data(a, buff, sizeof(buff)));
			assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
			assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
			assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		}
	}
}

/* The plaintext and its CRC are valid, but the 31-byte pack stream is
 * missing part of its final AES block. Do not stop after its first byte.
 */
DEFINE_TEST(test_read_format_7zip_encryption_truncated)
{
	const char *ref = "test_read_format_7zip_encryption_truncated.7z";
	struct archive *a;
	struct archive_entry *ae;
	char buff[16];

	if (!can7zipEncryption()) {
		skipping("7-Zip AES decryption is not supported on this platform");
		return;
	}
	extract_reference_file(ref);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_7zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_add_passphrase(a, "12345678"));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, ref, 7));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_data(a, buff, sizeof(buff)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
