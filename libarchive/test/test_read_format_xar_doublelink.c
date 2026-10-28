/*-
 * Copyright (c) 2024 Martin Matuska
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

#define __LIBARCHIVE_BUILD

/*
 * The outer file is declared as a hardlink origin, a nested file is then
 * declared as another origin, and the outer file is declared again.  The
 * nesting moves the outer file away from the head of the origin list, so a
 * head-only duplicate check permits the final declaration to make a cycle.
 */
static const unsigned char nested_doublelink[] = {
	0x78, 0x61, 0x72, 0x21, 0x00, 0x1c, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x6a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd2,
	0x00, 0x00, 0x00, 0x00, 0x78, 0x9c, 0x8d, 0x8e, 0x51, 0x0a, 0x80, 0x20,
	0x10, 0x44, 0xaf, 0x22, 0x7b, 0x80, 0xb6, 0xfa, 0xde, 0xd6, 0xb3, 0x58,
	0x59, 0x2c, 0x99, 0x86, 0x49, 0xd4, 0xed, 0x4b, 0x21, 0xe8, 0xb3, 0xaf,
	0x61, 0x98, 0xf7, 0x60, 0x48, 0x9f, 0xab, 0x53, 0x87, 0x8d, 0xbb, 0x04,
	0xdf, 0x41, 0x53, 0xd5, 0xa0, 0x99, 0x4e, 0x13, 0x99, 0x52, 0x18, 0x98,
	0x26, 0x71, 0x56, 0xc9, 0xf8, 0x2c, 0xc0, 0xe4, 0xcd, 0x6a, 0xd9, 0x10,
	0x96, 0xa4, 0x74, 0x6d, 0x56, 0x39, 0xf1, 0x4b, 0x07, 0x21, 0xca, 0x2c,
	0xde, 0x38, 0xe0, 0xcc, 0x13, 0xe6, 0xe9, 0xe3, 0xb6, 0xaf, 0xdb, 0xff,
	0x76, 0x31, 0x97, 0xff, 0x1c, 0x96, 0xb3, 0x98, 0x7f, 0xdf, 0xf6, 0x8f,
	0x47, 0x3b
};

DEFINE_TEST(test_read_format_xar_doublelink)
{
	const char *refname = "test_read_format_xar_doublelink.xar";
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_file(refname);

	/* Verify with seeking reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
        if(ARCHIVE_OK != archive_read_support_format_xar(a)) {
                skipping("XAR format unsupported");
                assertEqualInt(ARCHIVE_OK, archive_read_free(a));
                return;
        }
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname,
	    10240));

	assertA(ARCHIVE_FATAL == archive_read_next_header(a, &ae));
	assertEqualString(archive_error_string(a),
		"File with multiple link attributes");
	assert(archive_errno(a) != 0);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Verify that nesting cannot bypass the duplicate-link check. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_xar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a,
	    nested_doublelink, sizeof(nested_doublelink)));
	assertA(ARCHIVE_FATAL == archive_read_next_header(a, &ae));
	assertEqualString(archive_error_string(a),
	    "File with multiple link attributes");
	assert(archive_errno(a) != 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
