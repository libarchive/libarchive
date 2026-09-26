/*-
 * Copyright (c) 2026 krishna28238-arch
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
#include "test.h"

/*
 * A size vint with all ten continuation bits set cannot be decoded
 * into a 64-bit value at all.  The unpatched reader could not tell
 * this apart from a clean end of file, because read_var() returned
 * the same value for both, and it reported ARCHIVE_EOF for the
 * truncated archive instead of an error.
 *
 * The patched reader reports ARCHIVE_FATAL for the invalid vint.
 */
DEFINE_TEST(test_read_format_rar5_varint_overflow)
{
	/* RAR5 signature, a dummy CRC32 (the reader fails before
	 * validating it), and a size vint of ten 0xFF bytes. */
	static const uint8_t data[] = {
		0x52,0x61,0x72,0x21,0x1a,0x07,0x01,0x00,
		0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0xff
	};

	struct archive *a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_rar5(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a,
	    data, sizeof(data)));

	struct archive_entry *ae;
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertA(archive_error_string(a) != NULL &&
	    strstr(archive_error_string(a),
	        "varint value is too large") != NULL);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
