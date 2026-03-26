/* shrink */

// #if HAVE_LEGACY

#include "archive_platform.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "archive_read_private.h"
#include "archive_zip_legacy.h"

struct shrink_dictionary {
	uint16_t next;
	uint8_t byte;
	uint8_t flag;
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
	uint16_t next_node;
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
	*desc = calloc(1, sizeof(**desc));
	if (*desc == NULL) {
		return errno;
	}

	(*desc)->arch = a;
	(*desc)->cmp_size = cmp_size;
	(*desc)->bits = 0;
	(*desc)->num_bits = 0;
	(*desc)->code_size = 9;
	(*desc)->old_code = -1;
	(*desc)->next_node = 0;
	(*desc)->last_byte = 0;
	(*desc)->outstr_size = 0;
	(*desc)->outstr_start = 0;

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
	if ((code >= desc->next_node + 257)
	||  (code >= 257 && desc->dictionary[code - 257].next == 0xFFFF)) {
		end_byte = desc->last_byte;
		code = desc->old_code;
	}

	// Determine the length of the output string
	length = 1;
	index = code;
	while (index >= 257) {
		if (index == 0xFFFF) {
			return file_inconsistent;
		}
		++length;
		index = desc->dictionary[index - 257].next;
	}

	// Write the string in the opposite order from how the nodes are linked
	i = length;
	index = code;
	while (index >= 257) {
		str[--i] = desc->dictionary[index - 257].byte;
		index = desc->dictionary[index - 257].next;
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
	unsigned empty = 0xFFFF;

	/* Look for an empty node */
	for (uint16_t i = 0; i < desc->next_node; ++i) {
		if (empty == 0xFFFF && desc->dictionary[i].next == 0xFFFF) {
			empty = i;
		}
	}

	/* If no empty node, use the next unused one */
	if (empty == 0xFFFF) {
		/* Don't overflow the array */
		if (desc->next_node >= sizeof(desc->dictionary)/sizeof(desc->dictionary[0])) {
			return;
		}
		empty = desc->next_node++;
	}

	/* Add the new entry */
	desc->dictionary[empty].next = code;
	desc->dictionary[empty].byte = byte;
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
			for (unsigned i = 0; i < desc->next_node; ++i) {
				desc->dictionary[i].flag = 0;
			}
			for (unsigned i = 0; i < desc->next_node; ++i) {
				unsigned j = desc->dictionary[i].next;
				if (j >= 257) {
					desc->dictionary[j - 257].flag = 1;
				}
			}
			for (unsigned i = 0; i < desc->next_node; ++i) {
				if (!desc->dictionary[i].flag) {
					desc->dictionary[i].next = 0xFFFF;
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

// #endif /* HAVE_LEGACY */
