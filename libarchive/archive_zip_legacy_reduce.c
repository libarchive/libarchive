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

#include "archive_platform.h"

#if HAVE_LEGACY

#include <assert.h>

#if HAVE_ERRNO_H
#  include <errno.h>
#endif
#if HAVE_STDINT_H
#  include <stdint.h>
#endif
#if HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#if HAVE_STRING_H
#  include <string.h>
#endif

#include "archive_zip_legacy.h"

/* Decompressor state */
struct follower_set {
	uint8_t size;
	uint8_t bits;
	uint8_t chr[63];
};

struct reduce_desc {
	/* Source of bytes */
	struct arch_data arch;

	/* Current state of sliding window */
	struct lz77_window lz77;

	/* Compression level */
	unsigned level:2; /* 0-3 for level 1-4 */
	uint8_t length_mask;

	/* Follower set encoding */
	struct follower_set folset[256];
	uint8_t last_ch;
};

static int read_follower_set(struct reduce_desc *desc,
	struct follower_set *folset);
static int reduce_read_byte(struct reduce_desc *desc, uint8_t *byte);

int
reduce_init(struct reduce_desc **desc, struct archive_read *a,
	uint64_t cmp_size, struct trad_enc_ctx *decrypt, unsigned level,
	uint64_t *cmp_bytes_read)
{
	int err = 0;

	assert(1 <= level && level <= 4);

	if (*desc == NULL) {
		*desc = calloc(1, sizeof(**desc));
		if (*desc == NULL) {
			return errno;
		}
	}

	(*desc)->arch.arch = a;
	(*desc)->arch.cmp_size = cmp_size;
	(*desc)->arch.decrypt = decrypt;
	(*desc)->arch.bits = 0;
	(*desc)->arch.num_bits = 0;
	(*desc)->level = level - 1;
	(*desc)->length_mask = 255 >> level;
	(*desc)->last_ch = 0;
	err = lz77_init(&(*desc)->lz77, 0x100 << level);
	if (err) {
		reduce_free(desc);
		return err;
	}

	/* Read the follower sets */
	for (unsigned j = 256; j-- != 0; ) {
		err = read_follower_set(*desc, &(*desc)->folset[j]);
		if (err) {
			goto fail;
		}
	}

	*cmp_bytes_read = cmp_size - (*desc)->arch.cmp_size;
	return 0;

fail:
	*cmp_bytes_read = cmp_size - (*desc)->arch.cmp_size;
	return err;
}

void
reduce_free(struct reduce_desc **desc)
{
	lz77_free(&(*desc)->lz77);
	free(*desc);
	*desc = NULL;
}

int
reduce_read(struct reduce_desc *desc, uint8_t bytes[], size_t num_bytes,
	size_t *bytes_read, uint64_t *cmp_bytes_read)
{
	int err = 0;
	uint64_t cmp_size = desc->arch.cmp_size;
	size_t b_read;

	b_read = 0;
	while (b_read < num_bytes) {
		uint8_t byte;
		size_t count;

		/* Fulfill any pending copy */
		count = lz77_copy(&desc->lz77, bytes + b_read, num_bytes - b_read);
		b_read += count;

		if (b_read >= num_bytes) {
			break;
		}

		/* desc->copy_size is 0 at this point */
		/* Decode one byte from the follower set encoding */
		err = reduce_read_byte(desc, &byte);
		if (err) {
			goto fail;
		}

		if (byte != 0x90) {
			/* Literal byte */
			lz77_add_byte(&desc->lz77, byte);
		} else {
			/* May be a copy marker or a literal 0x90 */
			err = reduce_read_byte(desc, &byte);
			if (err) {
				goto fail;
			}
			if (byte == 0x00) {
				/* Literal 0x90 */
				lz77_add_byte(&desc->lz77, 0x90);
			} else {
				/* Copy marker */
				unsigned distance = byte >> (7 - desc->level);
				unsigned length = byte & desc->length_mask;
				if (length == desc->length_mask) {
					/* Need another length byte */
					err = reduce_read_byte(desc, &byte);
					if (err) {
						goto fail;
					}
					length += byte;
				}
				length += 3;
				err = reduce_read_byte(desc, &byte);
				if (err) {
					goto fail;
				}
				distance = (distance << 8) + byte + 1;
				lz77_set_copy(&desc->lz77, distance, length);
			}
		}
	}

	*bytes_read = b_read;
	*cmp_bytes_read = cmp_size - desc->arch.cmp_size;
	return 0;

fail:
	/* If we reach the end of the compressed data, return success with the
	   number of bytes read so far */
	*bytes_read = b_read;
	*cmp_bytes_read = cmp_size - desc->arch.cmp_size;
	return err == end_of_data ? ARCHIVE_EOF : err;
}

static int
read_follower_set(struct reduce_desc *desc,
	struct follower_set *folset)
{
	unsigned size;
	int err;
	unsigned i;

	/* Size of follower set */
	err = archive_read_bits(&desc->arch, 6, &size);
	if (err) {
		return err;
	}
	folset->size = size;

	/* Size in terms of bits (e.g. 64 -> 6 bits) */
	for (i = 1; (1U << i) < size; ++i) { /*nothing needed here*/ }
	folset->bits = i;

	/* Read the set of bytes */
	for (i = 0; i < size; ++i) {
		unsigned byte;
		err = archive_read_bits(&desc->arch, 8, &byte);
		if (err) {
			return err;
		}
		folset->chr[i] = byte;
	}

	return 0;
}

/* Read one byte from the follower set encoding */
static int
reduce_read_byte(struct reduce_desc *desc, uint8_t *byte)
{
	unsigned literal;
	int err;

	if (desc->folset[desc->last_ch].size == 0) {
		/* Always literal if the follower set is empty */
		literal = 1;
	} else {
		/* If the follower set is empty, the next bit determines
		   whether a literal follows */
		err = archive_read_bits(&desc->arch, 1, &literal);
		if (err) {
			return err;
		}
	}
	if (literal) {
		unsigned bits;
		err = archive_read_bits(&desc->arch, 8, &bits);
		if (err) {
			return err;
		}
		assert(bits <= 255);
		*byte = (uint8_t)bits;
	} else {
		unsigned index;
		err = archive_read_bits(&desc->arch, desc->folset[desc->last_ch].bits, &index);
		if (err) {
			return err;
		}
		if (index >= desc->folset[desc->last_ch].size) {
			return file_inconsistent;
		}
		*byte = desc->folset[desc->last_ch].chr[index];
	}
	desc->last_ch = *byte;
	return 0;
}

#endif /* HAVE_LEGACY */
