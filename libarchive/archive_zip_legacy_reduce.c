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

/* Current state of sliding window */
struct lz77_window {
	uint8_t *window;
	unsigned window_pos;
	unsigned copy_pos;
	unsigned copy_size;
	unsigned window_mask;
};

struct reduce_desc {
	/* Source of bytes */
	struct archive_read *arch;
	uint64_t cmp_size;
	unsigned bits;
	uint8_t num_bits;

	/* Current state of sliding window */
	struct lz77_window lz77;

	/* Compression level */
	uint8_t level;
	uint8_t length_mask;

	/* Follower set encoding */
	struct follower_set folset[256];
	uint8_t last_ch;
};

/* Errors occurring within the ZIP format */
enum {
	end_of_data = -1,
	file_truncated = -2,
	file_inconsistent = -3
};

static int read_follower_set(struct reduce_desc *desc,
	struct follower_set *folset);
static int reduce_read_byte(struct reduce_desc *desc, unsigned *byte);
static int read_bits(struct reduce_desc *desc, unsigned num_bits,
	unsigned *bits);

static int lz77_init(struct lz77_window *lz77, unsigned window_size);
static void lz77_free(struct lz77_window *lz77);
static size_t lz77_copy(struct lz77_window *lz77, uint8_t bytes[], size_t num_bytes);
static void lz77_add_byte(struct lz77_window *lz77, uint8_t byte);
static void lz77_set_copy(struct lz77_window *lz77, unsigned distance, unsigned length);

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

	*cmp_bytes_read = cmp_size - (*desc)->cmp_size;
	return 0;

fail:
	*cmp_bytes_read = cmp_size - (*desc)->cmp_size;
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
	size_t *bytes_read, size_t *cmp_bytes_read)
{
	int err = 0;
	uint64_t cmp_size = desc->cmp_size;
	size_t b_read;

	b_read = 0;
	while (b_read < num_bytes) {
		unsigned byte;
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
			bytes[b_read++] = byte;
			lz77_add_byte(&desc->lz77, byte);
		} else {
			/* May be a copy marker or a literal 0x90 */
			err = reduce_read_byte(desc, &byte);
			if (err) {
				goto fail;
			}
			if (byte == 0x00) {
				/* Literal 0x90 */
				bytes[b_read++] = 0x90;
				lz77_add_byte(&desc->lz77, 0x90);
			} else {
				/* Copy marker */
				unsigned distance = byte >> (8 - desc->level);
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
	*cmp_bytes_read = cmp_size - desc->cmp_size;
	return 0;

fail:
	/* If we reach the end of the compressed data, return success with the
	   number of bytes read so far */
	*bytes_read = b_read;
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

/* Read one byte from the follower set encoding */
static int
reduce_read_byte(struct reduce_desc *desc, unsigned *byte)
{
	unsigned literal;
	int err;

	if (desc->folset[desc->last_ch].size == 0) {
		/* Always literal if the follower set is empty */
		literal = 1;
	} else {
		/* If the follower set is empty, the next bit determines
		   whether a literal follows */
		err = read_bits(desc, 1, &literal);
		if (err) {
			return err;
		}
	}
	if (literal) {
		err = read_bits(desc, 8, byte);
		if (err) {
			return err;
		}
	} else {
		unsigned index;
		err = read_bits(desc, desc->folset[desc->last_ch].bits, &index);
		if (err) {
			return err;
		}
		*byte = desc->folset[desc->last_ch].chr[index];
	}
	desc->last_ch = *byte;
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
