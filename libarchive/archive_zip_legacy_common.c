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

#include "archive_read_private.h"
#include "archive_zip_legacy.h"

static void lz77_copy_window(struct lz77_window *lz77);

const char *
zip_legacy_error(int err)
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
lz77_init(struct lz77_window *lz77, unsigned window_size)
{
	assert(window_size * 2 <= sizeof(lz77->window));

	/* A copy from before the start of the uncompressed data is supposed
	   to copy zero bytes */
	memset(lz77->window, 0, window_size);
	lz77->window_pos = window_size;
	lz77->copy_pos = window_size;
	lz77->window_size = window_size;
	return 0;
}

void
lz77_free(struct lz77_window *lz77)
{
	/* no op */
	(void)lz77;
}

/* Fulfill any pending copy */
size_t
lz77_copy(struct lz77_window *lz77, uint8_t bytes[], size_t num_bytes)
{
	unsigned pending = lz77->window_pos - lz77->copy_pos;
	size_t b_read = num_bytes;

	if (b_read > pending) {
		b_read = pending;
	}

	memcpy(bytes, lz77->window + lz77->copy_pos, b_read);
	lz77->copy_pos += b_read;

	return b_read;
}

/* Add a byte to the sliding window */
void
lz77_add_byte(struct lz77_window *lz77, uint8_t byte)
{
	if (lz77->window_pos >= sizeof(lz77->window)) {
		lz77_copy_window(lz77);
	}
	lz77->window[lz77->window_pos++] = byte;
	/* lz77_copy will copy the byte to the final output */
}

/* Begin a copy */
void
lz77_set_copy(struct lz77_window *lz77, unsigned distance, unsigned length)
{
	/*
	 * The source and destination are allowed to overlap. The desired
	 * output is a repeating pattern. memmove does the wrong thing in this
	 * case, and memcpy invokes undefined behavior.
	 * Implement the repeating pattern with repeated calls to memcpy.
	 */
	while (length != 0) {
		unsigned block = (length < distance) ? length : distance;

		if (lz77->window_pos + block > sizeof(lz77->window)) {
			lz77_copy_window(lz77);
		}

		assert(lz77->window_pos + block <= sizeof(lz77->window));
		memcpy(lz77->window + lz77->window_pos,
		       lz77->window + lz77->window_pos - distance,
		       block);
		lz77->window_pos += block;

		length -= block;
	}
}

/* Copy the sliding window back to the start */
static void
lz77_copy_window(struct lz77_window *lz77)
{
    assert(lz77->window_pos >= lz77->window_size);

    unsigned start = lz77->window_pos - lz77->window_size;
    memmove(lz77->window, lz77->window + start, lz77->window_size);
    lz77->window_pos -= start;
    lz77->copy_pos -= start;
}

/* Read one or more bits from the archive */
int
archive_read_bits(struct arch_data *arch, unsigned num_bits, unsigned *bits)
{
	if (arch->num_bits < num_bits) {
		unsigned num_bytes = (num_bits - arch->num_bits + 7) / 8;
		const uint8_t *ptr;

		if (arch->cmp_size < num_bytes) {
			return end_of_data;
		}
		ptr = archive_read_bytes(arch, num_bytes);
		if (ptr == NULL) {
			return file_truncated;
		}
		for (unsigned i = 0; i < num_bytes; ++i) {
			arch->bits |= ptr[i] << arch->num_bits;
			arch->num_bits += 8;
		}
	}

	*bits = arch->bits;
	arch->bits >>= num_bits;
	arch->num_bits -= num_bits;
	*bits &= (1 << num_bits) - 1;

	return 0;
}

/* Read and possibly decrypt one or more bytes */
void const *
archive_read_bytes(struct arch_data *arch, unsigned num_bytes)
{
	void const *ptr;
	ssize_t avail;

	ptr = __archive_read_ahead(arch->arch, num_bytes, &avail);
	if (ptr == NULL) {
		return NULL;
	}
	__archive_read_consume(arch->arch, num_bytes);
	arch->cmp_size -= num_bytes;

	if (arch->decrypt) {
		trad_enc_decrypt_update(arch->decrypt,
			ptr, num_bytes,
			arch->decrypt_buf, num_bytes);
		ptr = arch->decrypt_buf;
	}

	return ptr;
}

#endif /* HAVE_LEGACY */
