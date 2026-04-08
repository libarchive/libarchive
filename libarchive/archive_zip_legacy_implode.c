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

#define SIZE(x) (sizeof(x)/sizeof((x)[0]))

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

/* State of the decoder */
enum implode_state {
	LITERAL_SIZE,  /* Expecting size of literal tree */
	LITERAL_DATA,  /* Expecting bytes of literal tree */
	LENGTH_SIZE,   /* Expecting size of length tree */
	LENGTH_DATA,   /* Expecting bytes of length tree */
	DISTANCE_SIZE, /* Expecting size of distance tree */
	DISTANCE_DATA, /* Expecting bytes of distance tree */
	/* Once we get to the UNIMPLODE state, we don't go back to
	   the above states */
	UNIMPLODE,          /* Expecting literal flag */
	READ_LITERAL,       /* Expecting literal */
	READ_DISTANCE_LOW,  /* Expecting low part of copy distance */
	READ_DISTANCE_HIGH, /* Expecting high part of copy distance */
	READ_LENGTH_FIRST,  /* Expecting first part of copy length */
	READ_LENGTH_SECOND  /* Expecting second part of copy length */
};

struct implode_desc {
	/* To read bits not on a byte boundary */
	struct arch_bits bits;
	/* Current state of sliding window */
	struct lz77_window lz77;
	/* Flags set in the Zip structure */
	uint8_t window_8k;
	uint8_t have_literal_tree;
	/* Current state of Shannon-Fano decoder */
	struct implode_tree literal_tree[256];
	struct implode_tree length_tree[64];
	struct implode_tree distance_tree[64];
	/* State of the decoder */
	enum implode_state state;
	/* For reading the Shannon-Fano trees */
	unsigned tree_size;
	unsigned tree_read;
	uint8_t tree_data[256];
	/* For reading literals and copy markers */
	unsigned tree_index;
	/* A copy marker */
	unsigned distance;
	unsigned length;
};

static int implode_setup(struct implode_desc *desc, struct zip_legacy_io *io);
static int read_tree(
	struct implode_tree tree[], unsigned num_values,
	uint8_t const tree_data[], unsigned tree_bytes);
static int set_bit_lengths(
	unsigned num_values,
	uint8_t bit_lengths[256],
	uint8_t const tree_data[256],
	unsigned tree_bytes);
static int build_codes(
	unsigned num_values,
	uint16_t codes[256],
	uint8_t const bit_lengths[256]);
static int build_tree(
	unsigned num_values,
	struct implode_tree tree[],
	uint16_t const codes[256],
	uint8_t const bit_lengths[256]);
static int read_literal_flag(struct implode_desc *desc, struct zip_legacy_io *io);
static int read_literal(struct implode_desc *desc, struct zip_legacy_io *io);
static int read_distance_low(struct implode_desc *desc, struct zip_legacy_io *io);
static int read_distance_high(struct implode_desc *desc, struct zip_legacy_io *io);
static int read_length_first(struct implode_desc *desc, struct zip_legacy_io *io);
static int read_length_second(struct implode_desc *desc, struct zip_legacy_io *io);
static int sf_decode(struct implode_desc *desc, struct zip_legacy_io *io,
	struct implode_tree const tree[], unsigned *elem);

/* Initialize the implode_desc structure and read the Shannon-Fano trees */
int
implode_init(struct implode_desc **desc, unsigned zip_flags)
{
	int err = 0;

	if (*desc == NULL) {
		*desc = calloc(1, sizeof(**desc));
		if (*desc == NULL) {
			return errno;
		}
	}

	(*desc)->bits.bits = 0;
	(*desc)->bits.num_bits = 0;
	(*desc)->window_8k = (zip_flags & 0x02) != 0;
	(*desc)->have_literal_tree = (zip_flags & 0x04) != 0;
	err = lz77_init(&(*desc)->lz77, (*desc)->window_8k ? 0x2000 : 0x1000);
	if (err) {
		implode_free(desc);
		return err;
	}
	(*desc)->state = (*desc)->have_literal_tree ? LITERAL_SIZE : LENGTH_SIZE;

	return 0;
}

void
implode_free(struct implode_desc **desc)
{
	lz77_free(&(*desc)->lz77);
	free(*desc);
	*desc = NULL;
}

int
implode_read(struct implode_desc *desc, struct zip_legacy_io *io)
{
	int eodata = 0;
	size_t total_in = io->total_in;
	size_t total_out = io->total_out;
	unsigned num_bits = desc->bits.num_bits;

	/* Set up the Shannon-Fano trees at the start */
	if (desc->state < UNIMPLODE) {
		int err = implode_setup(desc, io);
		if (err) {
			return err;
		}
	}
	if (desc->state < UNIMPLODE) {
		/* We're out of input but the trees are not complete */
		eodata = 1;
	}

	/* If we get here with eodata = 0, the Shannon-Fano trees are read and
	   we are ready to decompress */

	while (!eodata && io->total_out < io->avail_out) {
		size_t count;

		/* Fulfill any pending copy */
		count = lz77_copy(&desc->lz77, io->next_out + io->total_out,
			io->avail_out - io->total_out);
		io->total_out += count;

		if (io->total_out >= io->avail_out) {
			break;
		}

		switch (desc->state) {
		/* "Ground state" of the decoder */
		/* Read a single bit: 1 indicates a literal, 0 a copy marker */
		case UNIMPLODE:
			eodata = read_literal_flag(desc, io);
			/* May go to READ_LITERAL or READ_DISTANCE_LOW */
			break;

		/* Read a literal */
		case READ_LITERAL:
			eodata = read_literal(desc, io);
			/* Produces output and returns to UNIMPLODE */
			break;

		/* The remaining states read a copy marker */
		/* Read the low bits of the distance */
		case READ_DISTANCE_LOW:
			eodata = read_distance_low(desc, io);
			/* Goes to READ_DISTANCE_HIGH */
			break;

		/* Read the high bits of the distance */
		case READ_DISTANCE_HIGH:
			eodata = read_distance_high(desc, io);
			/* Goes to READ_LENGTH_FIRST */
			break;

		/* Read the first part of the distance */
		case READ_LENGTH_FIRST:
			eodata = read_length_first(desc, io);
			/* Goes to READ_LENGTH_SECOND */
			break;

		/* Read the second part of the distance */
		case READ_LENGTH_SECOND:
			eodata = read_length_second(desc, io);
			/* Produces output and returns to UNIMPLODE */
			break;

		default:
			assert(0);
			break;
		}
	}

	if (total_in == io->total_in && total_out == io->total_out
	&&  num_bits == desc->bits.num_bits) {
		return ARCHIVE_EOF;
	}
	return ARCHIVE_OK;
}

/* All states less than UNIMPLODE: Read the Shannon-Fano trees */
static int
implode_setup(struct implode_desc *desc, struct zip_legacy_io *io)
{
	while (desc->state < UNIMPLODE && io->total_in < io->avail_in) {
		size_t avail;
		int err = 0;

		/* Read the tree data */
		switch (desc->state) {
		case LITERAL_SIZE:
		case LENGTH_SIZE:
		case DISTANCE_SIZE:
			/* Read byte count for current tree */
			desc->tree_size = io->next_in[io->total_in++] + 1;
			desc->tree_read = 0;
			break;

		case LITERAL_DATA:
		case LENGTH_DATA:
		case DISTANCE_DATA:
			/* Read bytes for current tree */
			avail = io->avail_in - io->total_in;
			if (avail > desc->tree_size - desc->tree_read) {
				avail = desc->tree_size - desc->tree_read;
			}
			memcpy(desc->tree_data + desc->tree_read,
			       io->next_in + io->total_in,
			       avail);
			io->total_in += avail;
			desc->tree_read += avail;
			break;

		default:
			assert(0);
			break;
		}

		/* Change the state if warranted */
		switch (desc->state) {
		case LITERAL_SIZE:
			desc->state = LITERAL_DATA;
			break;

		case LITERAL_DATA:
			if (desc->tree_read >= desc->tree_size) {
				err = read_tree(
					desc->literal_tree, SIZE(desc->literal_tree),
					desc->tree_data, desc->tree_size);
				desc->state = LENGTH_SIZE;
			}
			break;

		case LENGTH_SIZE:
			desc->state = LENGTH_DATA;
			break;

		case LENGTH_DATA:
			if (desc->tree_read >= desc->tree_size) {
				err = read_tree(
					desc->length_tree, SIZE(desc->length_tree),
					desc->tree_data, desc->tree_size);
				desc->state = DISTANCE_SIZE;
			}
			break;

		case DISTANCE_SIZE:
			desc->state = DISTANCE_DATA;
			break;

		case DISTANCE_DATA:
			if (desc->tree_read >= desc->tree_size) {
				err = read_tree(
					desc->distance_tree, SIZE(desc->distance_tree),
					desc->tree_data, desc->tree_size);
				desc->state = UNIMPLODE;
			}
			break;

		default:
			assert(0);
			break;
		}

		if (err) {
			return err;
		}
	}

	return 0;
}

/* Read one Shannon-Fano tree from the archive */
static int
read_tree(struct implode_tree tree[], unsigned num_values,
	  uint8_t const tree_data[], unsigned tree_bytes)
{
	uint8_t bit_lengths[256];
	uint16_t codes[256];
	int err;

	/* Set the bit lengths */
	err = set_bit_lengths(num_values, bit_lengths, tree_data, tree_bytes);
	if (err) {
		return err;
	}

	/* Construct the codes */
	err = build_codes(num_values, codes, bit_lengths);
	if (err) {
		return err;
	}

	/* Build the tree */
	err = build_tree(num_values, tree, codes, bit_lengths);
	if (err) {
		return err;
	}

	return 0;
}

/*
 * Given the raw bytes for the Shannon-Fano tree as read from the compressed
 * data, produce an array of bit lengths.
 * The lengths are not sorted as described in APPNOTE.TXT; rather, build_codes
 * and build_tree will scan the array multiple times to produce the codes.
 */
static int
set_bit_lengths(
	unsigned num_values,
	uint8_t bit_lengths[256],
	uint8_t const tree_data[256],
	unsigned tree_bytes)
{
	unsigned i = 0;
	for (unsigned j = 0; j < tree_bytes; ++j) {
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

	return 0;
}

/*
 * Given the bit lengths from set_bit_lengths, produce the corresponding codes.
 * The codes are aligned at the left side of the 16 bit array elements.
 */
static int
build_codes(
	unsigned num_values,
	uint16_t codes[256],
	uint8_t const bit_lengths[256])
{
	unsigned next_code = 0x10000;
	unsigned bit = 0x8000;
	for (unsigned i = 1; i <= 16; ++i) {
		for (unsigned j = 0; j < num_values; ++j) {
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

	return 0;
}

/*
 * Given the codes from build_codes and the bit lengths from set_bit_lengths,
 * construct the final tree.
 */
static int
build_tree(unsigned num_values, struct implode_tree tree[],
	uint16_t const codes[256], uint8_t const bit_lengths[256])
{
	unsigned tree_size;
	unsigned bit;

	for (unsigned i = 0; i < num_values; ++i) {
		tree[i].next[0] = 0xFFFF;
		tree[i].next[1] = 0xFFFF;
	}
	tree_size = 1;
	for (unsigned i = 0; i < num_values; ++i) {
		uint16_t code = codes[i];
		uint8_t length = bit_lengths[i];
		unsigned node = 0;
		for (unsigned j = 0; j + 1 < length; ++j) {
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

/* UNIMPLODE state: read the literal flag */
static int
read_literal_flag(struct implode_desc *desc, struct zip_legacy_io *io)
{
	unsigned literal;

	int eodata = archive_read_bits(&desc->bits, io, 1, &literal);
	if (!eodata) {
		if (literal) {
			/* Read a literal */
			desc->state = READ_LITERAL;
		} else {
			/* Read a copy marker */
			desc->state = READ_DISTANCE_LOW;
		}
		/* Either way, we may need to use a Shannon-Fano tree */
		desc->tree_index = 0;
	}

	return eodata;
}

/* READ_LITERAL state: read a literal */
static int
read_literal(struct implode_desc *desc, struct zip_legacy_io *io)
{
	int eodata;
	unsigned byte;

	if (desc->have_literal_tree) {
		eodata = sf_decode(desc, io, desc->literal_tree, &byte);
	} else {
		eodata = archive_read_bits(&desc->bits, io, 8, &byte);
	}
	if (!eodata) {
		lz77_add_byte(&desc->lz77, byte);
		desc->state = UNIMPLODE;
	}
	return eodata;
}

/* READ_DISTANCE_LOW state: read the low bits of the distance */
static int
read_distance_low(struct implode_desc *desc, struct zip_legacy_io *io)
{
	int eodata = archive_read_bits(&desc->bits, io, desc->window_8k ? 7 : 6, &desc->distance);
	if (!eodata) {
		desc->state = READ_DISTANCE_HIGH;
		/* Next state will use the Shannon-Fano tree for distance */
		desc->tree_index = 0;
	}
	return eodata;
}

/* READ_DISTANCE_HIGH state: read the high bits of the distance */
static int
read_distance_high(struct implode_desc *desc, struct zip_legacy_io *io)
{
	unsigned dist_high;
	int eodata = sf_decode(desc, io, desc->distance_tree, &dist_high);
	if (!eodata) {
		/* Complete distance */
		desc->distance = (dist_high << (desc->window_8k ? 7 : 6))
			       + desc->distance + 1;
		desc->state = READ_LENGTH_FIRST;
		/* Next state will use the Shannon-Fano tree for length */
		desc->tree_index = 0;
	}
	return eodata;
}

/* READ_LENGTH_FIRST state: read the first part of the length */
static int
read_length_first(struct implode_desc *desc, struct zip_legacy_io *io)
{
	int eodata = sf_decode(desc, io, desc->length_tree, &desc->length);
	if (!eodata) {
		desc->state = READ_LENGTH_SECOND;
	}
	return eodata;
}

/* READ_LENGTH_SECOND state: read the second part of the length, and set the copy */
static int
read_length_second(struct implode_desc *desc, struct zip_legacy_io *io)
{
	if (desc->length == 63) {
		unsigned length2;
		int eodata = archive_read_bits(&desc->bits, io, 8, &length2);
		if (eodata) {
			return eodata;
		}
		desc->length += length2;
	}
	/* otherwise consumes no input */

	desc->length += desc->have_literal_tree ? 3 : 2;
	lz77_set_copy(&desc->lz77, desc->distance, desc->length);
	desc->state = UNIMPLODE;
	return 0;
}

/* Decode an element through a Shannon-Fano tree */
/* Return 0 on success, -1 on end of data, positive on file error */
static int
sf_decode(struct implode_desc *desc, struct zip_legacy_io *io,
	struct implode_tree const tree[], unsigned *elem)
{
	unsigned bit;

	while (1) {
		unsigned node2;
		int eodata = archive_read_bits(&desc->bits, io, 1, &bit);
		if (eodata) {
			return eodata;
		}
		node2 = tree[desc->tree_index].next[bit];
		if (node2 < 0x100) {
			*elem = node2;
			return 0;
		}
		if (node2 == 0xFFFF) {
			return file_inconsistent;
		}
		desc->tree_index = node2 - 0x100;
	}
}

#endif /* HAVE_LEGACY */
