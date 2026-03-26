/* implode */

// #if HAVE_LEGACY

#include "archive_platform.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "archive_read_private.h"
#include "archive_zip_legacy.h"

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
	/* Flags set in the Zip structure */
	uint8_t window_8k;
	uint8_t have_literal_tree;
	/* Current state of Shannon-Fano decoder */
	unsigned bits;
	uint8_t num_bits;
	struct implode_tree literal_tree[255];
	struct implode_tree length_tree[63];
	struct implode_tree distance_tree[63];
	/* Current state of sliding window */
	uint8_t window[8192];
	uint16_t window_pos;
	uint16_t copy_pos;
	uint16_t copy_size;
	uint16_t window_mask;
};

/* Errors occurring within the ZIP format */
enum {
	end_of_data = -1,
	file_truncated = -2,
	file_inconsistent = -3
};

static int read_tree(struct implode_desc *desc, unsigned num_values, struct implode_tree tree[]);
static void add_byte(struct implode_desc *desc, uint8_t byte);
static int sf_decode(struct implode_desc *desc, struct implode_tree const tree[],
	uint8_t *elem);
static int read_bits(struct implode_desc *desc, unsigned num_bits,
	uint8_t *bits);

/* Initialize the implode_desc structure and read the Shannon-Fano trees */
int
implode_init(struct implode_desc **desc, struct archive_read *a,
	uint64_t cmp_size, unsigned zip_flags, size_t *cmp_bytes_read)
{
	int err = 0;

	*desc = calloc(1, sizeof(**desc));
	if (*desc == NULL) {
		return errno;
	}

	(*desc)->arch = a;
	(*desc)->cmp_size = cmp_size;
	(*desc)->window_8k = (zip_flags & 0x02) != 0;
	(*desc)->have_literal_tree = (zip_flags & 0x04) != 0;
	(*desc)->bits = 0;
	(*desc)->num_bits = 0;
	(*desc)->window_pos = 0;
	(*desc)->copy_pos = 0;
	(*desc)->copy_size = 0;
	(*desc)->window_mask = (*desc)->window_8k ? 0x1FFF : 0x0FFF;

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
	memset((*desc)->window, 0, sizeof((*desc)->window));

	*cmp_bytes_read = cmp_size - (*desc)->cmp_size;
	return 0;

fail:
	*cmp_bytes_read = cmp_size - (*desc)->cmp_size;
	return err;
}

void
implode_free(struct implode_desc **desc)
{
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

	*bytes_read = 0;
	while (num_bytes != 0) {
		uint8_t literal;

		/* Fulfill any pending copy */
		while (desc->copy_size != 0 && num_bytes != 0) {
			uint8_t byte = desc->window[desc->copy_pos];
			bytes[0] = byte;
			++bytes;
			--num_bytes;
			++(*bytes_read);
			desc->copy_pos = (desc->copy_pos + 1) & desc->window_mask;
			--desc->copy_size;
			add_byte(desc, byte);
		}

		if (num_bytes == 0) {
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
			int err;
			uint8_t byte;

			if (desc->have_literal_tree) {
				err = sf_decode(desc, desc->literal_tree, &byte);
			} else {
				err = read_bits(desc, 8, &byte);
			}
			if (err) {
				goto fail;
			}
			bytes[0] = byte;
			++bytes;
			--num_bytes;
			++(*bytes_read);
			add_byte(desc, byte);
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

			desc->copy_size = length;
			desc->copy_pos = (desc->window_pos - distance)
				       & desc->window_mask;
		}
	}

	*cmp_bytes_read = cmp_size - desc->cmp_size;
	return 0;

fail:
	/* If we reach the end of the compressed data, return success with the
	   number of bytes read so far */
	*cmp_bytes_read = cmp_size - desc->cmp_size;
	return err == end_of_data ? ARCHIVE_EOF : err;
}

/* Read one Shannon-Fano tree from the archive */
static int
read_tree(struct implode_desc *desc, unsigned num_values, struct implode_tree tree[])
{
	const uint8_t *ptr;
	ssize_t avail;
	int tree_bytes;
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
	for (i = 0; i < num_values+1; ++i) {
		tree[i].next[0] = 0xFFFF;
		tree[i].next[1] = 0xFFFF;
	}
	tree_size = 1;
	for (i = 0; i < num_values; ++i) {
		uint16_t code = codes[i];
		uint8_t length = bit_lengths[i];
		unsigned node = 0;
		unsigned bit;
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

/* Add a byte to the sliding window */
static void
add_byte(struct implode_desc *desc, uint8_t byte)
{
	desc->window[desc->window_pos] = byte;
	desc->window_pos = (desc->window_pos + 1) & desc->window_mask;
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
