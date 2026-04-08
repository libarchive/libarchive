/*-
 * Copyright (c) 2026 Ray Chason
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

#ifndef ARCHIVE_ZIP_LEGACY_H_INCLUDED
#define ARCHIVE_ZIP_LEGACY_H_INCLUDED

#ifdef HAVE_LEGACY

#include "archive_read_private.h"

/* Errors occurring within the ZIP format */
enum {
	end_of_data = -1,
	file_truncated = -2,
	file_inconsistent = -3
};

const char *zip_legacy_error(int err);

/* Input and output to decoders */
struct zip_legacy_io {
	const uint8_t *next_in;
	size_t avail_in;
	size_t total_in;
	uint8_t *next_out;
	size_t avail_out;
	size_t total_out;
};

/* To decrypt compressed data */
struct trad_enc_ctx;
void trad_enc_decrypt_update(struct trad_enc_ctx *ctx, const uint8_t *in,
    size_t in_len, uint8_t *out, size_t out_len);

/* Implode */
struct implode_desc;

int implode_init(struct implode_desc **desc, unsigned zip_flags);
void implode_free(struct implode_desc **desc);
int implode_read(struct implode_desc *desc, struct zip_legacy_io *io);

/* Shrink */
struct shrink_desc;

int shrink_init(struct shrink_desc **desc);
void shrink_free(struct shrink_desc **desc);
int shrink_read(struct shrink_desc *desc, struct zip_legacy_io *io);

/* Reduce */
struct reduce_desc;

int reduce_init(struct reduce_desc **desc, struct archive_read *a,
	uint64_t cmp_size, struct trad_enc_ctx *decrypt, unsigned level,
	uint64_t *cmp_bytes_read);
void reduce_free(struct reduce_desc **desc);
int reduce_read(struct reduce_desc *desc, uint8_t bytes[], size_t num_bytes,
	size_t *bytes_read, uint64_t *cmp_bytes_read);

/* Current state of sliding window */
struct lz77_window {
	uint8_t window[8192*2];
	unsigned window_pos;
	unsigned copy_pos;
	unsigned window_size;
};

int lz77_init(struct lz77_window *lz77, unsigned window_size);
void lz77_free(struct lz77_window *lz77);
size_t lz77_copy(struct lz77_window *lz77, uint8_t bytes[], size_t num_bytes);
void lz77_add_byte(struct lz77_window *lz77, uint8_t byte);
void lz77_set_copy(struct lz77_window *lz77, unsigned distance, unsigned length);

/* Archive data, remaining bytes, decryption */
struct arch_data {
	/* Archive data from caller */
	struct archive_read *arch;
	/* Compressed bytes remaining */
	uint64_t cmp_size;
	/* To read bits not on a byte boundary */
	uint32_t bits;
	uint8_t num_bits;
	/* Traditional PKZIP decryption */
	struct trad_enc_ctx *decrypt;
	uint8_t decrypt_buf[256];
};

int archive_read_bits(struct arch_data *arch, unsigned num_bits, unsigned *bits);
void const *archive_read_bytes(struct arch_data *arch, unsigned num_bytes);

#endif /* HAVE_LEGACY */

#endif /* !ARCHIVE_ZIP_LEGACY_H_INCLUDED */
