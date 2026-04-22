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

#if HAVE_ERRNO_H
#  include <errno.h>
#endif
#if HAVE_STDINT_H
#  include <stdint.h>
#endif
#if HAVE_STDIO_H
#  include <stdio.h>
#endif
#if HAVE_STDLIB_H
#  include <stdlib.h>
#endif

#include "archive_zip_legacy.h"

#define SIZE(x) (sizeof(x)/sizeof((x)[0]))

/*
 * The dictionary is built as codes are received. "byte" is the last character
 * of the string and "next" is the code for the rest of the string. The string
 * is built by chasing the "next" field until a literal (less than 257) is
 * found, and then outputting the "byte" fields in the reverse of the order in
 * which they were found. "flag" indicates used and free nodes, and while
 * clearing, also indicates nodes that will not be cleared.
 *
 * The index of a used node is the code that it stands for, minus 257.
 * Codes 0 through 255 are literal bytes, and 256 is an escape code used to
 * change the code size or partially clear the dictionary.
 *
 * The dictionary is an array of size 8192-257. The largest code size is 13,
 * and so there can never be more than 8192-257 nodes in the dictionary.
 *
 * Unused nodes are kept in a free list, where "next" is the index of the next
 * free node (not offset by 257 in this case). The free list is built at
 * initialization and again when the clear code is received.
 */
struct shrink_dictionary {
	uint16_t next;
	uint8_t byte;
	uint8_t flag;
};

/* Values for shrink_dictionary::flag */
enum {
	node_free,      /* Node is free */
	node_used,      /* Node is in use */
	node_parent     /* Temporary while clearing: a used node that will not be cleared */
};

struct shrink_desc {
	/* To read bits not on a byte boundary */
	struct arch_bits bits;
	/* Decompressor state */
	unsigned code_size;
	unsigned old_code;
	struct shrink_dictionary dictionary[8192 - 257];
	uint16_t free_list;
	uint8_t last_byte;
	uint8_t shifted;
	/* Output string */
	uint8_t outstr[8192];
	unsigned outstr_size;
	unsigned outstr_start;
};

static int process_unshifted(struct shrink_desc *desc, struct zip_legacy_io *io);
static int process_shifted(struct shrink_desc *desc, struct zip_legacy_io *io);
static int lookup(struct shrink_desc *desc, unsigned code);
static void add_string(struct shrink_desc *desc, uint16_t code, uint8_t byte);
static void clear_dictionary(struct shrink_desc *desc);

/* Initialize the shrink_desc structure */
int
shrink_init(struct shrink_desc **desc)
{
	if (*desc == NULL) {
		*desc = calloc(1, sizeof(**desc));
		if (*desc == NULL) {
			return errno;
		}
	}

	(*desc)->bits.bits = 0;
	(*desc)->bits.num_bits = 0;
	(*desc)->code_size = 9;
	(*desc)->old_code = 0xFFFF;
	(*desc)->last_byte = 0;
	(*desc)->shifted = 0;
	(*desc)->outstr_size = 0;
	(*desc)->outstr_start = 0;

	/* The dictionary is initially empty */
	(*desc)->free_list = 0;
	for (unsigned i = 0; i < SIZE((*desc)->dictionary) - 1; ++i) {
		(*desc)->dictionary[i].flag = node_free;
		(*desc)->dictionary[i].next = i + 1;
	}
	(*desc)->dictionary[SIZE((*desc)->dictionary) - 1].next = 0xFFFF;

	return ARCHIVE_OK;
}

void
shrink_free(struct shrink_desc **desc)
{
	free(*desc);
	*desc = NULL;
}

int
shrink_read(struct shrink_desc *desc, struct zip_legacy_io *io)
{
	int err = 0;
	size_t total_in = io->total_in;
	size_t total_out = io->total_out;
	unsigned num_bits = desc->bits.num_bits;

	while (!err && io->total_out < io->avail_out) {
		/* Return any partial string still pending */
		if (desc->outstr_size > desc->outstr_start) {
			size_t num_bytes = io->avail_out - io->total_out;
			unsigned outstr_size = desc->outstr_size - desc->outstr_start;
			if (outstr_size > num_bytes) {
				outstr_size = num_bytes;
			}
			memcpy(io->next_out + io->total_out,
			       desc->outstr + desc->outstr_start,
			       outstr_size);
			io->total_out += outstr_size;
			desc->outstr_start += outstr_size;
			if (io->total_out >= io->avail_out) {
				break;
			}
		}

		desc->outstr_start = 0;
		desc->outstr_size = 0;

		/* Read and decode */
		if (desc->shifted) {
			err = process_shifted(desc, io);
			/* Proceeds to unshifted state */
		} else {
			err = process_unshifted(desc, io);
			/* May go to shifted or unshifted state */
		}
	}

	if (err != 0 && err != end_of_data) {
		return err;
	}
	if (total_in == io->total_in && total_out == io->total_out
	&&  num_bits == desc->bits.num_bits) {
		return ARCHIVE_EOF;
	}
	return ARCHIVE_OK;
}

/* Read one code while in the unshifted state */
static int
process_unshifted(struct shrink_desc *desc, struct zip_legacy_io *io)
{
	unsigned new_code;
	int eodata = archive_read_bits(&desc->bits, io, desc->code_size, &new_code);
	if (eodata) {
		return end_of_data;
	}

	if (new_code == 256) {
		/* Go to shifted state */
		desc->shifted = 1;
		return 0;
	}

	if (desc->old_code == 0xFFFF) {
		/* First time through */
		desc->old_code = new_code;
		if (new_code >= 256) {
			return file_inconsistent;
		}
		desc->outstr[0] = new_code;
		desc->outstr_size = 1;
		desc->last_byte = new_code;
	} else {
		/* Look up string in dictionary */
		int err = lookup(desc, new_code);
		if (err != 0) {
			return err;
		}
		add_string(desc, desc->old_code, desc->outstr[0]);
		desc->old_code = new_code;
	}

	return 0;
}

/* Read one code while in the shifted state */
static int
process_shifted(struct shrink_desc *desc, struct zip_legacy_io *io)
{
	unsigned new_code;
	int eodata = archive_read_bits(&desc->bits, io, desc->code_size, &new_code);
	if (eodata) {
		return end_of_data;
	}

	switch (new_code) {
	case 1: /* Change the code size */
		if (desc->code_size >= 13) {
			return file_inconsistent;
		}
		++desc->code_size;
		break;

	case 2: /* Partial clearing */
		clear_dictionary(desc);
		break;

	default: /* Invalid code */
		return file_inconsistent;
	}

	desc->shifted = 0;
	return 0;
}

static int
lookup(struct shrink_desc *desc, unsigned code)
{
	uint8_t *str = desc->outstr;
	unsigned length;
	unsigned index;
	unsigned i;
	unsigned depth;
	int end_byte = -1;

	// If code equals the next node, issue last_byte at the end
	if (code >= 257 && desc->dictionary[code - 257].flag == node_free) {
		if (code - 257 != desc->free_list) {
			return file_inconsistent;
		}
		end_byte = desc->last_byte;
		code = desc->old_code;
	}

	// Determine the length of the output string
	length = 1;
	index = code;
	depth = 0;
	while (index >= 257) {
		if (desc->dictionary[index - 257].flag == node_free) {
			end_byte = desc->last_byte;
			if (index == desc->old_code) {
				return file_inconsistent;
			}
			index = desc->old_code;
		} else {
			index = desc->dictionary[index - 257].next;
			++length;
		}
		++depth;
		if (depth >= SIZE(desc->dictionary)) {
			// Can't happen unless the tree contains cycles
			return file_inconsistent;
		}
	}

	// Write the string in the opposite order from how the nodes are linked
	i = length;
	index = code;
	while (index >= 257) {
		if (desc->dictionary[index - 257].flag == node_free) {
			end_byte = desc->last_byte;
			index = desc->old_code;
		} else {
			str[--i] = desc->dictionary[index - 257].byte;
			index = desc->dictionary[index - 257].next;
		}
	}
	str[0] = index;

	// Update last_byte for code equal to the next node
	if (end_byte >= 0) {
		str[length++] = end_byte;
	}
	desc->last_byte = str[0];

	desc->outstr_start = 0;
	desc->outstr_size = length;

	return 0;
}

static void
add_string(struct shrink_desc *desc, uint16_t code, uint8_t byte)
{
	unsigned empty;

	/* Use the first empty node */
	empty = desc->free_list;
	if (empty == 0xFFFF) {
		/* Dictionary is full */
		return;
	}
	desc->free_list = desc->dictionary[empty].next;

	/* Add the new entry */
	desc->dictionary[empty].next = code;
	desc->dictionary[empty].byte = byte;
	desc->dictionary[empty].flag = node_used;
}

static void
clear_dictionary(struct shrink_desc *desc)
{
	for (unsigned i = 0; i < SIZE(desc->dictionary); ++i) {
		struct shrink_dictionary const *node = &desc->dictionary[i];
		if (node->flag != node_free) {
			unsigned j = node->next;
			if (j >= 257) {
				desc->dictionary[j - 257].flag = node_parent;
			}
		}
	}
	desc->free_list = 0xFFFF;
	for (unsigned i = SIZE(desc->dictionary); i-- != 0; ) {
		struct shrink_dictionary *node = &desc->dictionary[i];
		if (node->flag == node_used) {
			node->flag = node_free;
		} else if (node->flag == node_parent) {
			node->flag = node_used;
		}
		if (node->flag == node_free) {
			node->next = desc->free_list;
			desc->free_list = i;
		}
	}
}

#endif /* HAVE_LEGACY */
