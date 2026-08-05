/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

DEFINE_TEST(test_xattr_clear)
{
	struct archive_entry *entry;
	const char *xname;
	const void *xval;
	size_t xsize;

	assert((entry = archive_entry_new()) != NULL);

	/* Add an entry */
	archive_entry_xattr_add_entry(entry, "xattr1", "value1", 7);

	/* Set iterator to head */
	assertEqualInt(1, archive_entry_xattr_reset(entry));

	assertEqualInt(ARCHIVE_OK,
		archive_entry_xattr_next(entry, &xname, &xval, &xsize));
	assertEqualString(xname, "xattr1");
	assertEqualString(xval, "value1");
	assertEqualInt((int)xsize, 7);

	/* Reset iterator to head */
	assertEqualInt(1, archive_entry_xattr_reset(entry));

	/* Clear xattr list */
	archive_entry_xattr_clear(entry);

	assertEqualInt(ARCHIVE_WARN,
		archive_entry_xattr_next(entry, &xname, &xval, &xsize));
	assertEqualString(xname, NULL);
	assertEqualString(xval, NULL);
	assertEqualInt((int)xsize, 0);
	archive_entry_free(entry);
}
