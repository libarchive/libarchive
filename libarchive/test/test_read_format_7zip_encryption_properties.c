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
 * Cover omitted properties, a single SHA-256 round, direct-key mode, and
 * salt/IV lengths with a zero high flag and a nonzero size nibble.
 * Plaintext: "7zip encryption edge case\n". Salt: "salt".
 * IV bytes: 0..15 for salt_size, 0..11 for iv_size; otherwise no salt/IV.
 */
DEFINE_TEST(test_read_format_7zip_encryption_properties)
{
	const char *refs[] = {
	    "test_read_format_7zip_encryption_default_properties.7z",
	    "test_read_format_7zip_encryption_cycles0.7z",
	    "test_read_format_7zip_encryption_cycles63.7z",
	    "test_read_format_7zip_encryption_salt_size.7z",
	    "test_read_format_7zip_encryption_iv_size.7z"
	};
	const size_t blocks[] = {1, 7, 15, 16, 17, 10240};
	static const char expected[] = "7zip encryption edge case\n";
	struct archive *a;
	struct archive_entry *ae;
	char buff[64], *archive_data;
	size_t i, j, archive_size;

	if (!can7zipEncryption()) {
		skipping("7-Zip AES decryption is not supported on this platform");
		return;
	}
	for (i = 0; i < sizeof(refs) / sizeof(refs[0]); i++) {
		extract_reference_file(refs[i]);
		archive_data = slurpfile(&archive_size, "%s", refs[i]);
		assert(archive_data != NULL);
		if (archive_data == NULL)
			continue;
		for (j = 0; j < sizeof(blocks) / sizeof(blocks[0]); j++) {
			failure("%s, input chunk %lu", refs[i],
			    (unsigned long)blocks[j]);
			assert((a = archive_read_new()) != NULL);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_format_7zip(a));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_add_passphrase(a, "12345678"));
			assertEqualIntA(a, ARCHIVE_OK,
			    read_open_memory_seek(a, archive_data, archive_size, blocks[j]));
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
		free(archive_data);
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

DEFINE_TEST(test_read_format_7zip_encryption_password_encoding)
{
	static const char unicode_pw[] = "\xe4\xb8\xad\xe6\x96\x87\xf0\x9f\x99\x82";
	static const char *invalid[] = {
	    "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80",
	    "\xf0\x9f", "\x80", "\xe2\x28\xa1"
	};
	static const unsigned char unicode_key[32] = {
	    0x11, 0x3a, 0x16, 0xcf, 0xda, 0x2f, 0x65, 0xd3,
	    0xcd, 0xdd, 0x0e, 0x43, 0xcc, 0x97, 0x28, 0xdd,
	    0x7d, 0xa7, 0x84, 0x81, 0x6e, 0x42, 0x8d, 0x94,
	    0xfd, 0xf1, 0xdb, 0x5a, 0xe5, 0x7f, 0xd1, 0x62
	};
	static const unsigned char maximum_key[32] = {
	    0xb1, 0xdb, 0x27, 0xeb, 0x6a, 0x0f, 0xf5, 0xab,
	    0x01, 0xeb, 0x39, 0x5d, 0xd2, 0xf6, 0x52, 0xb6,
	    0xb5, 0x05, 0x05, 0xe5, 0xc7, 0x54, 0x1a, 0x01,
	    0x3c, 0x24, 0xc4, 0x2e, 0x57, 0xfb, 0x03, 0x5a
	};
	static const unsigned char surrogate_key[32] = {
	    0x85, 0x69, 0x51, 0x85, 0x81, 0xbd, 0x30, 0x99,
	    0xa7, 0x76, 0x98, 0x37, 0xcf, 0x79, 0x0a, 0xed,
	    0xb9, 0xe8, 0x32, 0x26, 0x26, 0xbf, 0x80, 0x60,
	    0x14, 0xfd, 0x5a, 0x2a, 0x38, 0x03, 0x44, 0x08
	};
	unsigned char actual[32];
	char pw[260];
	size_t i;

	if (!can7zipEncryption()) {
		skipping("7-Zip AES decryption is not supported on this platform");
		return;
	}
	/* Independent SHA-256 vectors include the UTF-16LE password and
	 * eight zero counter bytes, for a single derivation round. */
	assertEqualInt(0, archive_7z_kdf_sha256(unicode_pw, NULL, 0, 0,
	    actual, sizeof(actual)));
	assertEqualMem(unicode_key, actual, sizeof(actual));
	memset(pw, 'a', 256);
	pw[256] = '\0';
	assertEqualInt(0, archive_7z_kdf_sha256(pw, NULL, 0, 0,
	    actual, sizeof(actual)));
	assertEqualMem(maximum_key, actual, sizeof(actual));
	pw[256] = 'b';
	pw[257] = '\0';
	assert(archive_7z_kdf_sha256(pw, NULL, 0, 0, actual,
	    sizeof(actual)) != 0);
	memcpy(pw + 254, "\xf0\x9f\x99\x82", 5);
	assertEqualInt(0, archive_7z_kdf_sha256(pw, NULL, 0, 0,
	    actual, sizeof(actual)));
	assertEqualMem(surrogate_key, actual, sizeof(actual));
	memset(pw, 'a', 255);
	memcpy(pw + 255, "\xf0\x9f\x99\x82", 5);
	assert(archive_7z_kdf_sha256(pw, NULL, 0, 0, actual,
	    sizeof(actual)) != 0);
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
		assert(archive_7z_kdf_sha256(invalid[i], NULL, 0, 0, actual,
		    sizeof(actual)) != 0);
}

#if defined(ARCHIVE_HAS_SHA256)
static int kdf_failure_stage, kdf_updates, kdf_finals;

static int
kdf_test_init(archive_sha256_ctx *ctx)
{
	(void)ctx;
	return (kdf_failure_stage == 1 ? ARCHIVE_FATAL : ARCHIVE_OK);
}

static int
kdf_test_update(archive_sha256_ctx *ctx, const void *data, size_t size)
{
	(void)ctx;
	(void)data;
	(void)size;
	kdf_updates++;
	return (kdf_failure_stage == 2 || kdf_failure_stage == 6 ||
	    (kdf_failure_stage == 4 && kdf_updates == 4) ?
	    ARCHIVE_FATAL : ARCHIVE_OK);
}

static int
kdf_test_final(archive_sha256_ctx *ctx, void *md)
{
	(void)ctx;
	kdf_finals++;
	if (kdf_failure_stage == 3 || kdf_failure_stage == 6)
		return (ARCHIVE_FATAL);
	if (kdf_failure_stage == 5) {
		memset(md, 0x24, 16);
		return (ARCHIVE_FATAL);
	}
	memset(md, 0x42, 32);
	return (ARCHIVE_OK);
}
#endif

DEFINE_TEST(test_read_format_7zip_encryption_kdf_failures)
{
#if defined(ARCHIVE_HAS_SHA256)
	static const char *names[] = {
	    "success", "init", "first update", "final",
	    "last update", "partial final", "update and final"
	};
	static const int updates[] = {4, 0, 1, 4, 4, 4, 1};
	struct archive_digest digest = __archive_digest;
	unsigned char key[32], expected[32];
	int stage, r;

	digest.sha256init = kdf_test_init;
	digest.sha256update = kdf_test_update;
	digest.sha256final = kdf_test_final;
	for (stage = 0; stage < 7; stage++) {
		failure("SHA-256 failure: %s", names[stage]);
		kdf_failure_stage = stage;
		kdf_updates = kdf_finals = 0;
		memset(key, 0xa5, sizeof(key));
		memset(expected, stage == 0 ? 0x42 : 0xa5, sizeof(expected));
		/* Four rounds: an update failure must stop immediately. */
		r = __archive_7z_kdf_sha256(&digest, "password", NULL, 0, 2,
		    key, sizeof(key));
		assertEqualInt(stage == 0 ? 0 : -1, r);
		assertEqualMem(expected, key, sizeof(key));
		assertEqualInt(updates[stage], kdf_updates);
		assertEqualInt(stage == 1 ? 0 : 1, kdf_finals);
	}
#else
	skipping("SHA-256 is required for the KDF failure tests");
#endif
}

/* NIST SP 800-38A, F.2.6 CBC-AES256.Decrypt.
 * https://doi.org/10.6028/NIST.SP.800-38A
 * Check backend CBC chaining independently of the 7-Zip parser and KDF. */
DEFINE_TEST(test_read_format_7zip_encryption_aes_cbc)
{
	static const unsigned char key[32] = {
	    0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
	    0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
	    0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
	    0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
	};
	static const unsigned char iv[16] = {
	    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};
	static const unsigned char ciphertext[64] = {
	    0xf5, 0x8c, 0x4c, 0x04, 0xd6, 0xe5, 0xf1, 0xba,
	    0x77, 0x9e, 0xab, 0xfb, 0x5f, 0x7b, 0xfb, 0xd6,
	    0x9c, 0xfc, 0x4e, 0x96, 0x7e, 0xdb, 0x80, 0x8d,
	    0x67, 0x9f, 0x77, 0x7b, 0xc6, 0x70, 0x2c, 0x7d,
	    0x39, 0xf2, 0x33, 0x69, 0xa9, 0xd9, 0xba, 0xcf,
	    0xa5, 0x30, 0xe2, 0x63, 0x04, 0x23, 0x14, 0x61,
	    0xb2, 0xeb, 0x05, 0xe2, 0xc3, 0x9b, 0xe9, 0xfc,
	    0xda, 0x6c, 0x19, 0x07, 0x8c, 0x6a, 0x9d, 0x1b
	};
	static const unsigned char plaintext[64] = {
	    0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	    0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	    0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
	    0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
	    0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
	    0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
	    0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
	    0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
	};
	const size_t chunks[] = {16, 32, 64};
	archive_crypto_ctx ctx;
	unsigned char output[80], guard[16];
	size_t i, pass, offset, out_len;
	int r;

	if (!can7zipEncryption()) {
		skipping("7-Zip AES decryption is not supported on this platform");
		return;
	}
	memset(guard, 0xa5, sizeof(guard));
	for (i = 0; i < sizeof(chunks) / sizeof(chunks[0]); i++) {
		/* Fresh contexts must restart from the supplied IV, even after
		 * a preceding stream has advanced its chaining state. */
		for (pass = 0; pass < 2; pass++) {
			failure("AES-CBC chunk %lu, pass %lu",
			    (unsigned long)chunks[i], (unsigned long)pass);
			memset(&ctx, 0, sizeof(ctx));
			memset(output, 0xa5, sizeof(output));
			r = archive_decrypto_aes_cbc_init(&ctx, key, sizeof(key),
			    iv, sizeof(iv));
			assertEqualInt(0, r);
			if (r != 0)
				continue;
			for (offset = 0; offset < sizeof(ciphertext);
			    offset += chunks[i]) {
				out_len = chunks[i];
				r = archive_decrypto_aes_cbc_update(&ctx,
				    ciphertext + offset, chunks[i], output + offset,
				    &out_len);
				assertEqualInt(0, r);
				assertEqualInt(chunks[i], out_len);
				if (r != 0 || out_len != chunks[i])
					break;
			}
			assertEqualMem(plaintext, output, sizeof(plaintext));
			assertEqualMem(guard, output + sizeof(plaintext),
			    sizeof(guard));
			assertEqualInt(0, archive_decrypto_aes_cbc_release(&ctx));
		}
	}
}
