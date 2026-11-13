/*-
 * Copyright (c) 2026 The libarchive authors
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
 * The archive holds two 2048-byte entries ("f0" = 'a's, "f1" = 'b's),
 * each with sha1 archived and extracted checksums.  Reading only part
 * of an entry and moving on to the next one used to leak the digest
 * contexts of the abandoned entry (visible under LeakSanitizer).
 */
static const char *refname = "test_read_format_xar_partial_read.xar";

static struct archive *
open_ref(const char *p, size_t s)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	if (ARCHIVE_OK != archive_read_support_format_xar(a)) {
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return (NULL);
	}
	/* Small blocks, so one data block is less than an entry. */
	assertEqualIntA(a, ARCHIVE_OK,
	    read_open_memory_seek(a, p, s, 512));
	return (a);
}

DEFINE_TEST(test_read_format_xar_partial_read)
{
	struct archive *a;
	struct archive_entry *ae;
	const void *buff;
	size_t size;
	la_int64_t offset;
	char data[2048];
	char *p;
	size_t s;
	int i;

	extract_reference_file(refname);
	p = slurpfile(&s, "%s", refname);
	assert(p != NULL);

	if ((a = open_ref(p, s)) == NULL) {
		skipping("XAR format unsupported");
		free(p);
		return;
	}

	/* Read one block of each entry, abandoning the rest. */
	for (i = 0; i < 2; i++) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualString(i == 0 ? "f0" : "f1",
		    archive_entry_pathname(ae));
		assertEqualInt(2048, archive_entry_size(ae));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_data_block(a, &buff, &size, &offset));
		assert(size > 0 && size < 2048);
		assertEqualInt(0, offset);
		assertEqualInt(i == 0 ? 'a' : 'b', ((const char *)buff)[0]);
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Reading everything still verifies the checksums. */
	assert((a = open_ref(p, s)) != NULL);
	for (i = 0; i < 2; i++) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualInt(2048, archive_read_data(a, data, sizeof(data)));
		assertEqualInt(i == 0 ? 'a' : 'b', data[2047]);
		assertEqualInt(0, archive_read_data(a, data, sizeof(data)));
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(p);
}
