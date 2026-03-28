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
#include <stdio.h>
#include <stdlib.h>

#include "archive_read_private.h"
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
	/* Source of bytes */
	struct archive_read *arch;
	uint64_t cmp_size;
	/* Decompressor state */
	uint32_t bits;
	unsigned num_bits;
	unsigned code_size;
	int old_code;
	struct shrink_dictionary dictionary[8192 - 257];
	uint16_t free_list;
	uint8_t last_byte;
	/* Output string */
	uint8_t outstr[8192];
	unsigned outstr_size;
	unsigned outstr_start;
	/* Debug */
	uint64_t unc_size;
};

/* Errors occurring within the ZIP format */
enum {
	end_of_data = -1,
	file_truncated = -2,
	file_inconsistent = -3
};

static int lookup(struct shrink_desc *desc, int code);
static void add_string(struct shrink_desc *desc, uint16_t code, uint8_t byte);
static int read_code_with_escapes(struct shrink_desc *desc, int *code);
static int read_code(struct shrink_desc *desc, int *code);

/* Initialize the shrink_desc structure */
int
shrink_init(struct shrink_desc **desc, struct archive_read *a,
	uint64_t cmp_size, size_t *cmp_bytes_read)
{
	if (*desc == NULL) {
		*desc = calloc(1, sizeof(**desc));
		if (*desc == NULL) {
			return errno;
		}
	}

	(*desc)->arch = a;
	(*desc)->cmp_size = cmp_size;
	(*desc)->bits = 0;
	(*desc)->num_bits = 0;
	(*desc)->code_size = 9;
	(*desc)->old_code = -1;
	(*desc)->last_byte = 0;
	(*desc)->outstr_size = 0;
	(*desc)->outstr_start = 0;

	/* The dictionary is initially empty */
	(*desc)->free_list = 0;
	for (unsigned i = 0; i < SIZE((*desc)->dictionary) - 1; ++i) {
		(*desc)->dictionary[i].flag = node_free;
		(*desc)->dictionary[i].next = i + 1;
	}
	(*desc)->dictionary[SIZE((*desc)->dictionary) - 1].next = 0xFFFF;

	*cmp_bytes_read = 0;
	return ARCHIVE_OK;
}

void
shrink_free(struct shrink_desc **desc)
{
	free(*desc);
	*desc = NULL;
}

int
shrink_read(struct shrink_desc *desc, uint8_t bytes[], size_t num_bytes,
	size_t *bytes_read, size_t *cmp_bytes_read)
{
	uint64_t cmp_size = desc->cmp_size;
	int err = 0;

	*bytes_read = 0;
	while (1) {
		/* Return any partial string still pending */
		if (desc->outstr_size > desc->outstr_start) {
			unsigned outstr_size = desc->outstr_size - desc->outstr_start;
			if (outstr_size > num_bytes) {
				outstr_size = num_bytes;
			}
			memcpy(bytes, desc->outstr + desc->outstr_start, outstr_size);
			*bytes_read += outstr_size;
			desc->outstr_start += outstr_size;
			bytes += outstr_size;
			num_bytes -= outstr_size;
			if (num_bytes == 0) {
				break;
			}
		}

		desc->outstr_start = 0;
		desc->outstr_size = 0;

		/* Read and decode */
		if (desc->old_code == -1) {
			/* First time through */
			err = read_code_with_escapes(desc, &desc->old_code);
			if (err != 0) {
				goto fail;
			}
			if (desc->old_code >= 256) {
				err = file_inconsistent;
				goto fail;
			}
			desc->outstr[0] = desc->old_code;
			desc->outstr_size = 1;
			desc->last_byte = desc->old_code;
		} else {
			int new_code;

			err = read_code_with_escapes(desc, &new_code);
			if (err != 0) {
				goto fail;
			}
			err = lookup(desc, new_code);
			if (err != 0) {
				goto fail;
			}
			add_string(desc, desc->old_code, desc->outstr[0]);
			desc->old_code = new_code;
		}
	}

	*cmp_bytes_read = cmp_size - desc->cmp_size;
	return ARCHIVE_OK;

fail:
	/* If we reach the end of the compressed data, return success with the
	   number of bytes read so far */
	*cmp_bytes_read = cmp_size - desc->cmp_size;
	return err == end_of_data ? ARCHIVE_EOF : err;
}

const char *
shrink_error(int err)
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

static int
lookup(struct shrink_desc *desc, int code)
{
	uint8_t *str = desc->outstr;
	unsigned length;
	int index;
	unsigned i;
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
	while (index >= 257) {
		if (desc->dictionary[index - 257].flag == node_free) {
			end_byte = desc->last_byte;
			index = desc->old_code;
		} else {
			index = desc->dictionary[index - 257].next;
			++length;
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

/* Read a code from the file, and process any 256 escapes */
static int
read_code_with_escapes(struct shrink_desc *desc, int *code)
{
	while (1) {
		int code2;
		int err = read_code(desc, code);
		if (err != 0 || *code != 256) {
			/* Not an escape code */
			return err;
		}

		err = read_code(desc, &code2);
		if (err != 0) {
			return err;
		}

		switch (code2) {
		case 1: /* Change the code size */
			if (desc->code_size >= 13) {
				return file_inconsistent;
			}
			++desc->code_size;
			break;

		case 2: /* Partial clearing */
			for (unsigned i = 0; i < SIZE(desc->dictionary); ++i) {
				struct shrink_dictionary *node = &desc->dictionary[i];
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
			break;

		default: /* Invalid code */
			return file_inconsistent;
		}
	}
}

/* Read a single code from the file */
static int
read_code(struct shrink_desc *desc, int *code)
{
	while (desc->num_bits < desc->code_size) {
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
		__archive_read_consume(desc->arch, 1);
		desc->num_bits += 8;
		--desc->cmp_size;
	}

	*code = desc->bits & ((1 << desc->code_size) - 1);
	desc->num_bits -= desc->code_size;
	desc->bits >>= desc->code_size;
	return ARCHIVE_OK;
}

#endif /* HAVE_LEGACY */
