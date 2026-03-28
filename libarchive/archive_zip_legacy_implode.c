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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "archive_read_private.h"
#include "archive_zip_legacy.h"

/* Current state of sliding window */
struct lz77_window {
	uint8_t *window;
	unsigned window_pos;
	unsigned copy_pos;
	unsigned copy_size;
	unsigned window_mask;
};

/* 5.3.7 and 5.3.8 */
struct implode_tree {
	/*
	 * Each bit indexes this array
	 * The value is 0-255 for a terminal value and 256-511 for an index
	 * to another node in the tree
	 * For 256-511, subtract 256 for the actual index
	 * 0xFFFF marks an invalid entry
	 */
	uint16_t next[2];
};

struct implode_desc {
	/* Source of bytes */
	struct archive_read *arch;
	uint64_t cmp_size;
	unsigned bits;
	uint8_t num_bits;
	/* Current state of sliding window */
	struct lz77_window lz77;
	/* Flags set in the Zip structure */
	uint8_t window_8k;
	uint8_t have_literal_tree;
	/* Current state of Shannon-Fano decoder */
	struct implode_tree literal_tree[256];
	struct implode_tree length_tree[64];
	struct implode_tree distance_tree[64];
};

/* Errors occurring within the ZIP format */
enum {
	end_of_data = -1,
	file_truncated = -2,
	file_inconsistent = -3
};

static int read_tree(struct implode_desc *desc, unsigned num_values, struct implode_tree tree[]);
static int sf_decode(struct implode_desc *desc, struct implode_tree const tree[],
	uint8_t *elem);
static int read_bits(struct implode_desc *desc, unsigned num_bits,
	uint8_t *bits);

static int lz77_init(struct lz77_window *lz77, unsigned window_size);
static void lz77_free(struct lz77_window *lz77);
static size_t lz77_copy(struct lz77_window *lz77, uint8_t bytes[], size_t num_bytes);
static void lz77_add_byte(struct lz77_window *lz77, uint8_t byte);
static void lz77_set_copy(struct lz77_window *lz77, unsigned distance, unsigned length);

/* Initialize the implode_desc structure and read the Shannon-Fano trees */
int
implode_init(struct implode_desc **desc, struct archive_read *a,
	uint64_t cmp_size, unsigned zip_flags, size_t *cmp_bytes_read)
{
	int err = 0;

	if (*desc == NULL) {
		*desc = calloc(1, sizeof(**desc));
		if (*desc == NULL) {
			return errno;
		}
	}

	(*desc)->arch = a;
	(*desc)->cmp_size = cmp_size;
	(*desc)->window_8k = (zip_flags & 0x02) != 0;
	(*desc)->have_literal_tree = (zip_flags & 0x04) != 0;
	(*desc)->bits = 0;
	(*desc)->num_bits = 0;
	err = lz77_init(&(*desc)->lz77, (*desc)->window_8k ? 0x2000 : 0x1000);
	if (err) {
		implode_free(desc);
		return err;
	}

	if ((*desc)->have_literal_tree) {
		err = read_tree((*desc), 256, (*desc)->literal_tree);
		if (err) {
			goto fail;
		}
	}
	err = read_tree((*desc), 64, (*desc)->length_tree);
	if (err) {
		goto fail;
	}
	err = read_tree((*desc), 64, (*desc)->distance_tree);
	if (err) {
		goto fail;
	}

	*cmp_bytes_read = cmp_size - (*desc)->cmp_size;
	return 0;

fail:
	*cmp_bytes_read = cmp_size - (*desc)->cmp_size;
	return err;
}

void
implode_free(struct implode_desc **desc)
{
	lz77_free(&(*desc)->lz77);
	free(*desc);
	*desc = NULL;
}

const char *
implode_error(int err)
{
	switch (err) {
	case end_of_data:
		return "End of compressed data";

	case file_truncated:
		return "Truncated ZIP file data";

	case file_inconsistent:
		return "Corrupted ZIP file data";

	default:
		return strerror(err);
	}
}

int
implode_read(struct implode_desc *desc, uint8_t bytes[], size_t num_bytes,
	size_t *bytes_read, size_t *cmp_bytes_read)
{
	int err = 0;
	uint64_t cmp_size = desc->cmp_size;
	size_t b_read;

	b_read = 0;
	while (b_read < num_bytes) {
		uint8_t literal;
		size_t count;

		/* Fulfill any pending copy */
		count = lz77_copy(&desc->lz77, bytes + b_read, num_bytes - b_read);
		b_read += count;

		if (b_read >= num_bytes) {
			break;
		}

		/* desc->copy_size is 0 at this point */
		/* Read one bit: literal or copy marker? */
		err = read_bits(desc, 1, &literal);
		if (err) {
			goto fail;
		}

		if (literal) {
			/* Literal byte found */
			uint8_t byte;

			if (desc->have_literal_tree) {
				err = sf_decode(desc, desc->literal_tree, &byte);
			} else {
				err = read_bits(desc, 8, &byte);
			}
			if (err) {
				goto fail;
			}
			bytes[b_read++] = byte;
			lz77_add_byte(&desc->lz77, byte);
		} else {
			/* Copy marker found */
			uint8_t dist_low, dist_high;
			uint8_t length1, length2;
			unsigned distance;
			unsigned length;

			/* Low bits of distance */
			err = read_bits(desc, desc->window_8k ? 7 : 6, &dist_low);
			if (err) {
				goto fail;
			}
			/* High bits of distance */
			err = sf_decode(desc, desc->distance_tree, &dist_high);
			if (err) {
				goto fail;
			}
			/* Complete distance */
			distance = (dist_high << (desc->window_8k ? 7 : 6))
				 + dist_low + 1;

			/* First part of length */
			err = sf_decode(desc, desc->length_tree, &length1);
			if (err) {
				goto fail;
			}
			length = length1;
			if (length1 == 63) {
				/* Second part of length */
				err = read_bits(desc, 8, &length2);
				if (err) {
					goto fail;
				}
				length += length2;
			}
			length += desc->have_literal_tree ? 3 : 2;

			lz77_set_copy(&desc->lz77, distance, length);
		}
	}

	*bytes_read = b_read;
	*cmp_bytes_read = cmp_size - desc->cmp_size;
	return 0;

fail:
	/* If we reach the end of the compressed data, return success with the
	   number of bytes read so far */
	*bytes_read = b_read;
	*cmp_bytes_read = cmp_size - desc->cmp_size;
	return err == end_of_data ? ARCHIVE_EOF : err;
}

/* Read one Shannon-Fano tree from the archive */
static int
read_tree(struct implode_desc *desc, unsigned num_values, struct implode_tree tree[])
{
	const uint8_t *ptr;
	ssize_t avail;
	unsigned tree_bytes;
	uint8_t tree_data[256];
	uint8_t bit_lengths[256];
	uint16_t codes[256];
	unsigned next_code, bit;
	unsigned i, j;
	unsigned tree_size;

	/* Raw data for the tree (5.3.7): */
	/* Number of bytes that encode the tree */
	if (desc->cmp_size == 0) {
		return file_inconsistent;
	}
	ptr = __archive_read_ahead(desc->arch, 1, &avail);
	if (ptr == NULL) {
		return file_truncated;
	}
	tree_bytes = ptr[0] + 1;
	__archive_read_consume(desc->arch, 1);
	--desc->cmp_size;

	/* The code counts and bit lengths */
	if (desc->cmp_size < tree_bytes) {
		return file_inconsistent;
	}
	ptr = __archive_read_ahead(desc->arch, tree_bytes, &avail);
	if (ptr == NULL) {
		return file_truncated;
	}
	desc->cmp_size -= tree_bytes;
	memcpy(tree_data, ptr, tree_bytes);
	__archive_read_consume(desc->arch, tree_bytes);

	/* Set the bit lengths */
	i = 0;
	for (j = 0; j < (unsigned)tree_bytes; ++j) {
		unsigned count = (tree_data[j] >> 4) + 1;
		unsigned length = (tree_data[j] & 0x0F) + 1;
		while (count != 0) {
			if (i >= num_values) {
				return file_inconsistent;
			}
			bit_lengths[i++] = length;
			--count;
		}
	}
	if (i != num_values) {
		return file_inconsistent;
	}

	/* Construct the codes */
	next_code = 0x10000;
	bit = 0x8000;
	for (i = 1; i <= 16; ++i) {
		for (j = 0; j < num_values; ++j) {
			if (bit_lengths[j] == i) {
				if (next_code < bit) {
					return file_inconsistent;
				}
				next_code -= bit;
				codes[j] = (uint16_t)next_code;
			}
		}
		bit >>= 1;
	}

	/* Build the tree */
	for (i = 0; i < num_values; ++i) {
		tree[i].next[0] = 0xFFFF;
		tree[i].next[1] = 0xFFFF;
	}
	tree_size = 1;
	for (i = 0; i < num_values; ++i) {
		uint16_t code = codes[i];
		uint8_t length = bit_lengths[i];
		unsigned node = 0;
		for (j = 0; j + 1 < length; ++j) {
			bit = (code >> (15 - j)) & 1;
			if (tree[node].next[bit] == 0xFFFF) {
				if (tree_size >= num_values - 1) {
					return file_inconsistent;
				}
				tree[node].next[bit] = tree_size + 0x100;
				++tree_size;
			}
			if (tree[node].next[bit] < 0x100) {
				return file_inconsistent;
			}
			node = tree[node].next[bit] - 0x100;
		}
		bit = (code >> (16 - length)) & 1;
		tree[node].next[bit] = i;
	}

	return 0;
}

/* Decode an element through a Shannon-Fano tree */
/* Return 0 on success, -1 on end of data, positive on file error */
static int
sf_decode(struct implode_desc *desc, struct implode_tree const tree[],
	uint8_t *elem)
{
	unsigned node = 0;
	uint8_t bit;
	int err;

	while (1) {
		unsigned node2;
		err = read_bits(desc, 1, &bit);
		if (err) {
			return err;
		}
		node2 = tree[node].next[bit];
		if (node2 < 0x100) {
			*elem = node2;
			return 0;
		}
		if (node2 == 0xFFFF) {
			return file_inconsistent;
		}
		node = node2 - 0x100;
	}
}

/* Read one or more bits from the archive */
static int
read_bits(struct implode_desc *desc, unsigned num_bits, uint8_t *bits)
{
	while (desc->num_bits < num_bits) {
		const uint8_t *ptr;
		ssize_t avail;

		if (desc->cmp_size == 0) {
			return end_of_data;
		}
		ptr = __archive_read_ahead(desc->arch, 1, &avail);
		if (ptr == NULL) {
			return file_truncated;
		}
		desc->bits |= ptr[0] << desc->num_bits;
		desc->num_bits += 8;
		--desc->cmp_size;
		__archive_read_consume(desc->arch, 1);
	}

	*bits = desc->bits;
	desc->bits >>= num_bits;
	desc->num_bits -= num_bits;
	*bits &= (1 << num_bits) - 1;

	return 0;
}

static int
lz77_init(struct lz77_window *lz77, unsigned window_size)
{
	free(lz77->window);
	lz77->window = calloc(1, window_size);
	if (lz77->window == NULL) {
		return errno;
	}
	lz77->window_pos = 0;
	lz77->copy_pos = 0;
	lz77->copy_size = 0;
	lz77->window_mask = window_size - 1;
	return 0;
}

static void
lz77_free(struct lz77_window *lz77)
{
	free(lz77->window);
	lz77->window = NULL;
}

/* Fulfill any pending copy */
static size_t
lz77_copy(struct lz77_window *lz77, uint8_t bytes[], size_t num_bytes)
{
	size_t b_read = 0;

	while (lz77->copy_size != 0 && b_read < num_bytes) {
		uint8_t byte = lz77->window[lz77->copy_pos];
		bytes[b_read++] = byte;
		lz77_add_byte(lz77, byte);
		lz77->copy_pos = (lz77->copy_pos + 1) & lz77->window_mask;
		--lz77->copy_size;
	}

	return b_read;
}

/* Add a byte to the sliding window */
static void
lz77_add_byte(struct lz77_window *lz77, uint8_t byte)
{
	lz77->window[lz77->window_pos] = byte;
	lz77->window_pos = (lz77->window_pos + 1) & lz77->window_mask;
}

/* Begin a copy */
static void
lz77_set_copy(struct lz77_window *lz77, unsigned distance, unsigned length)
{
	lz77->copy_size = length;
	lz77->copy_pos = (lz77->window_pos - distance) & lz77->window_mask;
}

#endif /* HAVE_LEGACY */
