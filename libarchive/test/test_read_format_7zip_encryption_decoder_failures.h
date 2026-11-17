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
#ifndef TEST_READ_FORMAT_7ZIP_ENCRYPTION_DECODER_FAILURES_H
#define TEST_READ_FORMAT_7ZIP_ENCRYPTION_DECODER_FAILURES_H

#include "archive_platform.h"
#include "archive.h"
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


int test_7zip_support(struct archive *);
#ifdef HAVE_LZMA_H
lzma_ret test_lzma_code(lzma_stream *, lzma_action);
#endif
#if defined(HAVE_BZLIB_H) && defined(BZ_CONFIG_ERROR)
int test_bz_decompress(bz_stream *);
int test_bz_end(bz_stream *);
#endif
#ifdef HAVE_ZLIB_H
int test_inflate(z_streamp, int);
#endif
#if HAVE_ZSTD_H && HAVE_LIBZSTD
size_t test_zstd_decompress(ZSTD_DStream *, ZSTD_outBuffer *, ZSTD_inBuffer *);
ZSTD_DStream *test_zstd_create(void);
#endif
int test_cbc_init(archive_crypto_ctx *, const uint8_t *, size_t,
    const uint8_t *, size_t);
int test_cbc_update(archive_crypto_ctx *, const uint8_t *, size_t,
    uint8_t *, size_t *);
int test_kdf(const char *, const uint8_t *, size_t, unsigned,
    uint8_t *, size_t);

#ifdef LIBARCHIVE_7ZIP_TEST_READER
#define lzma_code test_lzma_code
#define BZ2_bzDecompress test_bz_decompress
#define BZ2_bzDecompressEnd test_bz_end
#define inflate test_inflate
#define ZSTD_decompressStream test_zstd_decompress
#define ZSTD_createDStream test_zstd_create
#undef archive_decrypto_aes_cbc_init
#undef archive_decrypto_aes_cbc_update
#undef archive_7z_kdf_sha256
#define archive_decrypto_aes_cbc_init test_cbc_init
#define archive_decrypto_aes_cbc_update test_cbc_update
#define archive_7z_kdf_sha256 test_kdf
#define archive_read_support_format_7zip test_7zip_support
#endif
#endif
