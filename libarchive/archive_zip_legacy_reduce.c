/* reduce */

#include "archive_platform.h"

#if HAVE_LEGACY

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "archive_read_private.h"
#include "archive_zip_legacy.h"

struct reduce_desc;

/* Decompressor state */
struct follower_set {
	uint8_t size;
	uint8_t bits;
	uint8_t chr[63];
};

struct reduce_desc {
	/* Source of bytes */
	struct archive_read *arch;
	uint64_t cmp_size;

	unsigned bits;
	uint8_t num_bits;

	/* Compression level */
	uint8_t level;

	/* Follower set encoding */
	struct follower_set folset[256];
	uint8_t last_ch;

	/* State machine according to 5.2.5 */
	uint8_t state;
	uint8_t length_mask;
	uint8_t distance;
	uint16_t length;

	/* Current state of sliding window */
	uint8_t window[4096];
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

static void add_byte(struct reduce_desc *desc, uint8_t byte);
static int read_follower_set(struct reduce_desc *desc,
	struct follower_set *folset);
static int read_bits(struct reduce_desc *desc, unsigned num_bits,
	unsigned *bits);

int
reduce_init(struct reduce_desc **desc, struct archive_read *a,
	uint64_t cmp_size, unsigned level, size_t *cmp_bytes_read)
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
	(*desc)->bits = 0;
	(*desc)->num_bits = 0;
	(*desc)->level = level;
	(*desc)->last_ch = 0;
	(*desc)->state = 0;
	(*desc)->length = 0;
	(*desc)->length_mask = 255 >> level;
	(*desc)->distance = 0;
	(*desc)->window_pos = 0;
	(*desc)->copy_pos = 0;
	(*desc)->copy_size = 0;
	(*desc)->window_mask = (256 << level) - 1;
	memset((*desc)->window, 0, sizeof((*desc)->window));

	/* Read the follower sets */
	for (unsigned j = 256; j-- != 0; ) {
		err = read_follower_set(*desc, &(*desc)->folset[j]);
		if (err) {
			goto fail;
		}
	}

	*cmp_bytes_read = cmp_size - (*desc)->cmp_size;
	return 0;

fail:
	*cmp_bytes_read = cmp_size - (*desc)->cmp_size;
	return err;
}

void
reduce_free(struct reduce_desc **desc)
{
	free(*desc);
	*desc = NULL;
}

int
reduce_read(struct reduce_desc *desc, uint8_t bytes[], size_t num_bytes,
	size_t *bytes_read, size_t *cmp_bytes_read)
{
	int err = 0;
	uint64_t cmp_size = desc->cmp_size;

	*bytes_read = 0;
	while (num_bytes != 0) {
		unsigned byte;
		unsigned literal;
		unsigned length, distance;

		/* Fulfill any pending copy */
		while (desc->copy_size != 0 && num_bytes != 0) {
			byte = desc->window[desc->copy_pos];
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
		/* Decode one byte from the follower set encoding */
		if (desc->folset[desc->last_ch].size == 0) {
			/* Always literal if the follower set is empty */
			literal = 1;
		} else {
			/* If the follower set is empty, the next bit determines
			   whether a literal follows */
			err = read_bits(desc, 1, &literal);
			if (err) {
				goto fail;
			}
		}
		if (literal) {
			err = read_bits(desc, 8, &byte);
			if (err) {
				goto fail;
			}
		} else {
			unsigned index;
			err = read_bits(desc, desc->folset[desc->last_ch].bits, &index);
			if (err) {
				goto fail;
			}
			byte = desc->folset[desc->last_ch].chr[index];
		}
		desc->last_ch = byte;

		/* State machine according to 5.2.5 */
		switch (desc->state) {
		case 0: /* Normal state */
			if (byte == 0x90) {
				desc->state = 1;
			} else {
				bytes[0] = byte;
				++bytes;
				--num_bytes;
				++(*bytes_read);
				add_byte(desc, byte);
			}
			break;

		case 1: /* Escape byte received */
			if (byte != 0) {
				/* let V <- C */
				desc->distance = byte >> (8 - desc->level);
				/* let len <- L(V) */
				desc->length = byte & desc->length_mask;
				/* let state <- F(len) */
				desc->state = (desc->length == desc->length_mask)
					    ? 2 : 3;
			} else {
				/* Literal 0x90 */
				bytes[0] = 0x90;
				++bytes;
				--num_bytes;
				++(*bytes_read);
				add_byte(desc, 0x90);
				desc->state = 0;
			}
			break;

		case 2: /* Additional length */
			desc->length += byte;
			desc->state = 3;
			break;

		case 3: /* Additional distance */
			/* Copy marker is complete */
			distance = (desc->distance << 8) + byte + 1;
			length = desc->length + 3;
			desc->copy_size = length;
			desc->copy_pos = (desc->window_pos - distance)
				       & desc->window_mask;
			desc->state = 0;
			break;
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

const char *
reduce_error(int err)
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

/* Add a byte to the sliding window */
static void
add_byte(struct reduce_desc *desc, uint8_t byte)
{
	desc->window[desc->window_pos] = byte;
	desc->window_pos = (desc->window_pos + 1) & desc->window_mask;
}

static int
read_follower_set(struct reduce_desc *desc,
	struct follower_set *folset)
{
	unsigned size;
	int err;
	unsigned i;

	/* Size of follower set */
	err = read_bits(desc, 6, &size);
	if (err) {
		return err;
	}
	folset->size = size;

	/* Size in terms of bits (e.g. 64 -> 6 bits) */
	for (i = 1; (1U << i) < size; ++i) {}
	folset->bits = i;

	/* Read the set of bytes */
	for (i = 0; i < size; ++i) {
		unsigned byte;
		err = read_bits(desc, 8, &byte);
		if (err) {
			return err;
		}
		folset->chr[i] = byte;
	}

	return 0;
}

/* Read one or more bits from the archive */
static int
read_bits(struct reduce_desc *desc, unsigned num_bits, unsigned *bits)
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

#endif /* HAVE_LEGACY */
