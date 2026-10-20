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
#ifdef HAVE_LZMA_H
#include <lzma.h>
#endif
#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif
#ifdef HAVE_ZSTD_H
#include <zstd.h>
#include <zstd_errors.h>
#endif

/* Compile an isolated reader with decoder-call wrappers. The real library
 * and its public entry point remain unchanged. This exercises the complete
 * password-selection path without exhausting memory or adding production
 * fault-injection options. Only this translation unit sees the wrappers. */
static int test_7zip_support(struct archive *);
#ifdef HAVE_LZMA_H
static lzma_ret test_lzma_code(lzma_stream *, lzma_action);
#define lzma_code test_lzma_code
#endif
#if defined(HAVE_BZLIB_H) && defined(BZ_CONFIG_ERROR)
static int test_bz_decompress(bz_stream *);
static int test_bz_end(bz_stream *);
#define BZ2_bzDecompress test_bz_decompress
#define BZ2_bzDecompressEnd test_bz_end
#endif
#ifdef HAVE_ZLIB_H
static int test_inflate(z_streamp, int);
#define inflate test_inflate
#endif
#if HAVE_ZSTD_H && HAVE_LIBZSTD
static size_t test_zstd_decompress(ZSTD_DStream *, ZSTD_outBuffer *, ZSTD_inBuffer *);
static ZSTD_DStream *test_zstd_create(void);
#define ZSTD_decompressStream test_zstd_decompress
#define ZSTD_createDStream test_zstd_create
#endif
static int test_cbc_init(archive_crypto_ctx *, const uint8_t *, size_t,
    const uint8_t *, size_t);
static int test_cbc_update(archive_crypto_ctx *, const uint8_t *, size_t,
    uint8_t *, size_t *);
static int test_kdf(const char *, const uint8_t *, size_t, unsigned,
    uint8_t *, size_t);
#undef archive_decrypto_aes_cbc_init
#undef archive_decrypto_aes_cbc_update
#undef archive_7z_kdf_sha256
#define archive_decrypto_aes_cbc_init test_cbc_init
#define archive_decrypto_aes_cbc_update test_cbc_update
#define archive_7z_kdf_sha256 test_kdf
#define archive_read_support_format_7zip test_7zip_support
#include "../archive_read_support_format_7zip.c"
#undef archive_read_support_format_7zip
#undef lzma_code
#undef BZ2_bzDecompress
#undef BZ2_bzDecompressEnd
#undef inflate
#undef ZSTD_decompressStream
#undef ZSTD_createDStream

enum decoder_fault {
	FAULT_NONE, FAULT_MEMORY, FAULT_DATA, FAULT_INTERNAL,
	FAULT_MEMLIMIT, FAULT_CREATE, FAULT_CBC_INIT, FAULT_CBC_UPDATE,
	FAULT_KDF, FAULT_CLEANUP
};
static enum decoder_fault fault;
static int injections;

static int
test_cbc_init(archive_crypto_ctx *ctx, const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len)
{
	if (fault == FAULT_CBC_INIT) {
		injections++;
		return (-1);
	}
	return (__archive_cryptor.decrypto_aes_cbc_init(ctx, key, key_len, iv, iv_len));
}

static int
test_cbc_update(archive_crypto_ctx *ctx, const uint8_t *in, size_t in_len,
    uint8_t *out, size_t *out_len)
{
	if (fault == FAULT_CBC_UPDATE) {
		injections++;
		return (-1);
	}
	return (__archive_cryptor.decrypto_aes_cbc_update(ctx, in, in_len, out, out_len));
}

static int
test_kdf(const char *pw, const uint8_t *salt, size_t salt_len,
    unsigned cycles, uint8_t *key, size_t key_len)
{
	if (fault == FAULT_KDF) {
		injections++;
		return (-1);
	}
	return (__archive_cryptor.kdf_7z_sha256(pw, salt, salt_len, cycles, key, key_len));
}

#ifdef HAVE_LZMA_H
static lzma_ret
test_lzma_code(lzma_stream *stream, lzma_action action)
{
	if (fault >= FAULT_MEMORY && fault <= FAULT_MEMLIMIT) {
		injections++;
		switch (fault) {
		case FAULT_MEMORY: return (LZMA_MEM_ERROR);
		case FAULT_MEMLIMIT: return (LZMA_MEMLIMIT_ERROR);
		case FAULT_DATA: return (LZMA_DATA_ERROR);
		default: return (LZMA_PROG_ERROR);
		}
	}
	return (lzma_code(stream, action));
}
#endif
#if defined(HAVE_BZLIB_H) && defined(BZ_CONFIG_ERROR)
static int
test_bz_decompress(bz_stream *stream)
{
	if (fault >= FAULT_MEMORY && fault <= FAULT_MEMLIMIT) {
		injections++;
		switch (fault) {
		case FAULT_MEMORY: return (BZ_MEM_ERROR);
		case FAULT_DATA: return (BZ_DATA_ERROR);
		default: return (BZ_PARAM_ERROR);
		}
	}
	return (BZ2_bzDecompress(stream));
}

static int
test_bz_end(bz_stream *stream)
{
	if (fault == FAULT_CLEANUP && injections == 0) {
		injections++;
		return (BZ_PARAM_ERROR);
	}
	return (BZ2_bzDecompressEnd(stream));
}
#endif
#ifdef HAVE_ZLIB_H
static int
test_inflate(z_streamp stream, int flush)
{
	if (fault >= FAULT_MEMORY && fault <= FAULT_MEMLIMIT) {
		injections++;
		switch (fault) {
		case FAULT_MEMORY: return (Z_MEM_ERROR);
		case FAULT_DATA: return (Z_DATA_ERROR);
		default: return (Z_STREAM_ERROR);
		}
	}
	return (inflate(stream, flush));
}
#endif
#if HAVE_ZSTD_H && HAVE_LIBZSTD
static size_t
test_zstd_decompress(ZSTD_DStream *stream, ZSTD_outBuffer *out, ZSTD_inBuffer *in)
{
	if (fault >= FAULT_MEMORY && fault <= FAULT_MEMLIMIT) {
		injections++;
		switch (fault) {
		case FAULT_MEMORY: return ((size_t)-ZSTD_error_memory_allocation);
		case FAULT_DATA: return ((size_t)-ZSTD_error_corruption_detected);
		default: return ((size_t)-ZSTD_error_stage_wrong);
		}
	}
	return (ZSTD_decompressStream(stream, out, in));
}

static ZSTD_DStream *
test_zstd_create(void)
{
	if (fault == FAULT_CREATE) {
		injections++;
		return (NULL);
	}
	return (ZSTD_createDStream());
}
#endif

static const char *
failure_password(struct archive *a, void *data)
{
	int *calls = data;

	(void)a;
	++*calls;
	return (NULL);
}

static void
verify_decoder_failure(const char *ref, enum decoder_fault mode)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[4096];
	la_ssize_t n;
	size_t total = 0;
	int calls = 0, r;

	fault = FAULT_NONE;
	injections = 0;
	assert((a = archive_read_new()) != NULL);
	if (a == NULL)
		return;
	assertEqualIntA(a, ARCHIVE_OK, test_7zip_support(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_add_passphrase(a, "12345678"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_passphrase_callback(a, &calls, failure_password));
	if (!assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, ref, 10240)))
		goto cleanup;
	if (!assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae)))
		goto cleanup;
	/* Header parsing uses the real decoders; inject only while decoding
	 * encrypted file data with a known correct registered password. */
	fault = mode;
	while ((n = archive_read_data(a, buff, sizeof(buff))) > 0)
		total += (size_t)n;
	if (mode == FAULT_NONE) {
		assert(total > 0);
		assertEqualIntA(a, 0, n);
		while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
			while ((n = archive_read_data(a, buff, sizeof(buff))) > 0)
				total += (size_t)n;
			assertEqualIntA(a, 0, n);
		}
		assertEqualIntA(a, ARCHIVE_EOF, r);
		assertEqualInt(0, injections);
		assertEqualInt(0, calls);
	} else {
		assertEqualInt(0, total);
		assertEqualInt(1, injections);
		if (mode == FAULT_DATA) {
			assertEqualIntA(a, ARCHIVE_FAILED, n);
			assertEqualInt(1, calls);
			assertEqualString("Incorrect passphrase", archive_error_string(a));
		} else {
			if (mode == FAULT_CBC_INIT || mode == FAULT_KDF)
				assertEqualIntA(a, ARCHIVE_FAILED, n);
			else
				assertEqualIntA(a, ARCHIVE_FATAL, n);
			assertEqualInt(0, calls);
			if (mode == FAULT_MEMORY || mode == FAULT_MEMLIMIT || mode == FAULT_CREATE)
				assertEqualInt(ENOMEM, archive_errno(a));
			assert(archive_error_string(a) != NULL);
			if (archive_error_string(a) != NULL)
				assert(strcmp("Incorrect passphrase", archive_error_string(a)) != 0);
		}
	}
cleanup:
	fault = FAULT_NONE;
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* The BZip2 fixture contains the first 16 bytes of "ordinary valid boundary\n",
 * compressed with BZip2, then AES-256-CBC with password 12345678, salt "salt",
 * IV bytes 0..15 and KDF power 0. Its uncompressed header has a folder CRC.
 * The archive and plaintext were independently checked with 7-Zip. */
DEFINE_TEST(test_read_format_7zip_encryption_decoder_failures)
{
	const struct {
		const char *ref;
		const char *version;
		enum decoder_fault extra;
	} cases[] = {
	    {"test_read_format_7zip_encryption.7z", archive_liblzma_version(), FAULT_MEMLIMIT},
	    {"test_read_format_7zip_encryption_lzma2.7z", archive_liblzma_version(), FAULT_MEMLIMIT},
	    {"test_read_format_7zip_encryption_bzip2.7z", archive_bzlib_version(), FAULT_CLEANUP},
	    {"test_read_format_7zip_encryption_passphrase_deflate.7z", archive_zlib_version(), FAULT_NONE},
	    {"test_read_format_7zip_encryption_passphrase_zstd.7z", archive_libzstd_version(), FAULT_CREATE}
	};
	size_t i;

	if (!can7zipEncryption()) {
		skipping("7-Zip AES decryption is required");
		return;
	}
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		if (cases[i].version == NULL) {
			skipping("%s requires its decompression library", cases[i].ref);
			continue;
		}
		extract_reference_file(cases[i].ref);
		verify_decoder_failure(cases[i].ref, FAULT_NONE);
		verify_decoder_failure(cases[i].ref, FAULT_MEMORY);
		verify_decoder_failure(cases[i].ref, FAULT_DATA);
		verify_decoder_failure(cases[i].ref, FAULT_INTERNAL);
		if (cases[i].extra != FAULT_NONE)
			verify_decoder_failure(cases[i].ref, cases[i].extra);
	}
}

DEFINE_TEST(test_read_format_7zip_encryption_crypto_failures)
{
	const char *ref = "test_read_format_7zip_encryption_passphrase_copy.7z";

	if (!can7zipEncryption()) {
		skipping("7-Zip AES decryption is required");
		return;
	}
	extract_reference_file(ref);
	verify_decoder_failure(ref, FAULT_NONE);
	verify_decoder_failure(ref, FAULT_CBC_INIT);
	verify_decoder_failure(ref, FAULT_CBC_UPDATE);
	verify_decoder_failure(ref, FAULT_KDF);
}
