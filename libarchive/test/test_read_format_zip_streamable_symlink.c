/*-
 * Copyright (c) 2026 Sri Vedantam
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

DEFINE_TEST(test_read_format_zip_streamable_symlink)
{
	const char *refname = "test_read_format_zip_streamable_symlink.zip";
	struct archive_entry *entry;
	struct archive *a;
	char *archive_data, data[8];
	size_t archive_size;

	if (archive_zlib_version() == NULL) {
		skipping("zlib not available");
		return;
	}

	extract_reference_file(refname);
	archive_data = slurpfile(&archive_size, "%s", refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_streamable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_data, archive_size));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("link", archive_entry_pathname(entry));
	assertEqualInt(AE_IFLNK, archive_entry_filetype(entry));
	assertEqualString("target-path", archive_entry_symlink(entry));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString("visible", archive_entry_pathname(entry));
	assertEqualInt(7, archive_read_data(a, data, sizeof(data)));
	assertEqualMem("present", data, 7);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &entry));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(archive_data);
}
