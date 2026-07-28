/*-
 * Copyright (c) 2026 Tobias Stoeckmann
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
 * Regression test for NULL pointer dereference in ISO9660 writer (Issue #3235).
 *
 * When Rockridge deep-directory relocation moves a deep directory into
 * rr_moved, calculate_directory_descriptors() advances file->cur_content
 * to NULL.  Later, set_directory_record() dereferences file->cur_content
 * without a NULL check, causing a crash.
 *
 * The fix resets file->cur_content after the calculation loop.
 */
DEFINE_TEST(test_write_format_iso9660_rr_crash)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buff;
	size_t buffsize = 128 * 2048;
	size_t used;

	buff = malloc(buffsize);
	assert(buff != NULL);
	if (buff == NULL)
		return;

	/* ISO9660 format with Rockridge and ISO level 4. */
	assert((a = archive_write_new()) != NULL);
	assertA(0 == archive_write_set_format_iso9660(a));
	assertA(0 == archive_write_add_filter_none(a));
	assertA(0 == archive_write_set_options(a, "iso9660:iso-level=4"));
	assertA(0 == archive_write_set_bytes_per_block(a, 1));
	assertA(0 == archive_write_set_bytes_in_last_block(a, 1));
	assertA(0 == archive_write_open_memory(a, buff, buffsize, &used));

	/* Add a deep directory to trigger Rockridge relocation. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_atime(ae, 2, 0);
	archive_entry_set_ctime(ae, 4, 0);
	archive_entry_set_mtime(ae, 5, 0);
	archive_entry_copy_pathname(ae,
	    "d1/d2/d3/d4/d5/d6/d7/d8/d9/d10/d11/d12/d13/d14/d15/d16");
	archive_entry_set_mode(ae, S_IFDIR | 0755);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/*
	 * Close must not crash with NULL pointer dereference.
	 * It may return an error, but must not segfault.
	 */
	archive_write_close(a);
	archive_write_free(a);

	free(buff);
}
