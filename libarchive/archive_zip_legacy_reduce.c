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

enum folset_state {
	READ_FOLSET_SIZE,  /* Read size of follower set */
	READ_FOLSET_DATA,  /* Read contents of follower set */
	/* Once we get to the READ_LITERAL_FLAG state, we don't go back to
	   the above states */
	READ_LITERAL_FLAG, /* Read bit: 1 for literal, 0 to use follower set */
	READ_BYTE,         /* Read 8 bits for literal byte */
	READ_INDEX,        /* Read 1-6 bits for follower set index */
	PROCESS_BYTE       /* Process byte according to b_state */
};

enum byte_state {
	BYTE_LITERAL,      /* Byte is literal unless equal to 0x90 */
	BYTE_COPY1,        /* First byte of copy marker, or 0 for literal 0x90 */
	BYTE_COPY2,        /* Second byte of copy marker */
	BYTE_COPY3         /* Third byte of copy marker */
};

struct reduce_desc {
	/* To read bits not on a byte boundary */
	uint64_t bits;
	uint8_t num_bits;

	/* Current state of sliding window */
	struct lz77_window lz77;

	/* Compression level */
	unsigned level:2; /* 0-3 for level 1-4 */
	uint8_t length_mask;

	/* Follower set encoding */
	struct follower_set folset[256];
	uint8_t last_ch;
	uint8_t bytes_read;

	enum folset_state f_state;
	enum byte_state b_state;
	unsigned distance;
	unsigned length;
};

static int reduce_setup(struct reduce_desc *desc, struct zip_legacy_io *io);
static void process_byte(struct reduce_desc *desc);
static int archive_read_bits_2(struct reduce_desc *desc, struct zip_legacy_io *io,
	unsigned num_bits, unsigned *bits);

int
reduce_init(struct reduce_desc **desc, unsigned level)
{
	int err = 0;

	assert(1 <= level && level <= 4);

	if (*desc == NULL) {
		*desc = calloc(1, sizeof(**desc));
		if (*desc == NULL) {
			return errno;
		}
	}

	(*desc)->bits = 0;
	(*desc)->num_bits = 0;
	(*desc)->level = level - 1;
	(*desc)->length_mask = 255 >> level;
	(*desc)->last_ch = 0xFF;
	(*desc)->f_state = READ_FOLSET_SIZE;
	(*desc)->b_state = BYTE_LITERAL;
	err = lz77_init(&(*desc)->lz77, 0x100 << level);
	if (err) {
		reduce_free(desc);
	}

	return err;

	return 0;
}

void
reduce_free(struct reduce_desc **desc)
{
	lz77_free(&(*desc)->lz77);
	free(*desc);
	*desc = NULL;
}

int
reduce_read(struct reduce_desc *desc, struct zip_legacy_io *io)
{
	int eodata = 0;
	size_t total_in = io->total_in;
	size_t total_out = io->total_out;
	unsigned num_bits = desc->num_bits;

	/* Set up the follower sets at the start */
	if (desc->f_state < READ_LITERAL_FLAG) {
		int err = reduce_setup(desc, io);
		if (err) {
			return err;
		}
	}
	if (desc->f_state < READ_LITERAL_FLAG) {
		/* We're out of input but the follower sets are not complete */
		eodata = 1;
	}

	/* If we get here with eodata = 0, the follower sets are read and
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

		switch (desc->f_state) {
		/* "Ground state" for the follower set decoder */
		case READ_LITERAL_FLAG:
			if (desc->folset[desc->last_ch].size == 0) {
				/* Consume no input; read a literal byte */
				desc->f_state = READ_BYTE;
			} else {
				unsigned literal;

				eodata = archive_read_bits_2(desc, io, 1, &literal);
				if (eodata) {
					break;
				}
				if (literal) {
					desc->f_state = READ_BYTE;
				} else {
					desc->f_state = READ_INDEX;
				}
			}
			/* Proceed to READ_BYTE or READ_INDEX */
			break;

		case READ_BYTE:         /* Read 8 bits for literal byte */
			{
				unsigned byte;

				eodata = archive_read_bits_2(desc, io, 8, &byte);
				if (eodata) {
					break;
				}
				desc->last_ch = byte;
			}
			desc->f_state = PROCESS_BYTE;
			break;

		case READ_INDEX:        /* Read 1-6 bits for follower set index */
			{
				unsigned byte;

				eodata = archive_read_bits_2(desc, io, desc->folset[desc->last_ch].bits, &byte);
				if (eodata) {
					break;
				}
				if (byte >= desc->folset[desc->last_ch].size) {
					return file_inconsistent;
				}
				desc->last_ch = desc->folset[desc->last_ch].chr[byte];
			}
			desc->f_state = PROCESS_BYTE;
			break;

		case PROCESS_BYTE:      /* Process byte according to b_state */
			process_byte(desc);
			desc->f_state = READ_LITERAL_FLAG;
			break;

		default:
			assert(0);
		}
	}

	if (total_in == io->total_in && total_out == io->total_out
	&&  num_bits == desc->num_bits) {
		return ARCHIVE_EOF;
	}
	return ARCHIVE_OK;
}

static int
reduce_setup(struct reduce_desc *desc, struct zip_legacy_io *io)
{
	int eodata;

	while (desc->f_state < READ_LITERAL_FLAG && io->total_in < io->avail_in) {
		switch (desc->f_state) {
		case READ_FOLSET_SIZE:
			{
				unsigned i;
				unsigned size;

				eodata = archive_read_bits_2(desc, io, 6, &size);
				if (eodata) {
					return eodata;
				}
				desc->folset[desc->last_ch].size = size;
				for (i = 1; (1U << i) < size; ++i) { /*nothing needed here*/ }
				desc->folset[desc->last_ch].bits = i;
				desc->bytes_read = 0;
				desc->f_state = READ_FOLSET_DATA;
			}
			break;

		case READ_FOLSET_DATA:
			if (desc->folset[desc->last_ch].size != 0) {
				unsigned byte;

				eodata = archive_read_bits_2(desc, io, 8, &byte);
				if (eodata) {
					return eodata;
				}
				desc->folset[desc->last_ch].chr[desc->bytes_read++] = byte;
			}
			if (desc->bytes_read >= desc->folset[desc->last_ch].size) {
				if (desc->last_ch == 0) {
					/* All follower sets read */
					desc->f_state = READ_LITERAL_FLAG;
				} else {
					/* Read next follower set */
					--desc->last_ch;
					desc->bytes_read = 0;
					desc->f_state = READ_FOLSET_SIZE;
				}
			}
			break;

		default:
			assert(0);
		}
	}

	return 0;
}

static void
process_byte(struct reduce_desc *desc)
{
	uint8_t byte = desc->last_ch;
	unsigned distance;

	/* State machine is according to APPNOTE.TXT 5.2.5 */
	switch (desc->b_state) {
	case BYTE_LITERAL:      /* Byte is literal unless equal to 0x90 */
		if (byte == 0x90) {
			desc->b_state = BYTE_COPY1;
		} else {
			lz77_add_byte(&desc->lz77, byte);
			/* remain in BYTE_LITERAL */
		}
		break;

	case BYTE_COPY1:        /* First byte of copy marker, or 0 for literal 0x90 */
		if (byte == 0x00) {
			/* Literal 0x90 */
			lz77_add_byte(&desc->lz77, 0x90);
			desc->b_state = BYTE_LITERAL;
		} else {
			desc->distance = byte >> (7 - desc->level);
			desc->length = byte & desc->length_mask;
			if (desc->length == desc->length_mask) {
				desc->b_state = BYTE_COPY2;
			} else {
				desc->b_state = BYTE_COPY3;
			}
		}
		break;

	case BYTE_COPY2:        /* Second byte of copy marker */
		desc->length += byte;
		desc->b_state = BYTE_COPY3;
		break;

	case BYTE_COPY3:        /* Third byte of copy marker */
		distance = (desc->distance << 8) + byte + 1;
		lz77_set_copy(&desc->lz77, distance, desc->length + 3);
		desc->b_state = BYTE_LITERAL;
		break;
	}
}

/* Read the given number of bits, possibly not byte aligned */
/* Return -1 if end of data reached, else 0 */
static int
archive_read_bits_2(struct reduce_desc *desc, struct zip_legacy_io *io,
	unsigned num_bits, unsigned *bits)
{
	if (desc->num_bits < num_bits) {
		unsigned num_bytes = (num_bits - desc->num_bits + 7) / 8;

		if (io->total_in + num_bytes > io->avail_in) {
			num_bytes = (unsigned)(io->avail_in - io->total_in);
		}
		for (unsigned i = 0; i < num_bytes; ++i) {
			desc->bits |= io->next_in[io->total_in++] << desc->num_bits;
			desc->num_bits += 8;
		}
	}
	if (desc->num_bits < num_bits) {
		return -1;
	}

	*bits = desc->bits;
	desc->bits >>= num_bits;
	desc->num_bits -= num_bits;
	*bits &= (1 << num_bits) - 1;

	return 0;
}

#endif /* HAVE_LEGACY */
