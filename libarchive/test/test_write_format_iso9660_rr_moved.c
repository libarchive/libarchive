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
 * Directories nested deeper than eight levels are relocated into the
 * rr_moved directory when the archive is closed (Rock Ridge, RRIP
 * 4.1.5).  When two of the relocated directories share a basename,
 * the insertion into rr_moved fails.  On that error path the writer
 * used to leak the subtree it had already moved into the rejected
 * clone.
 */
DEFINE_TEST(test_write_format_iso9660_rr_moved)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buff;
	size_t buffsize = 256 * 2048;
	size_t used;

	buff = malloc(buffsize);
	assert(buff != NULL);
	if (buff == NULL)
		return;

	assert((a = archive_write_new()) != NULL);
	assertA(0 == archive_write_set_format_iso9660(a));
	assertA(0 == archive_write_add_filter_none(a));
	assertA(0 == archive_write_set_bytes_per_block(a, 1));
	assertA(0 == archive_write_set_bytes_in_last_block(a, 1));
	assertA(0 == archive_write_open_memory(a, buff, buffsize, &used));

	/* A path nested more than eight levels deep, with several
	 * directories sharing the name "a": more than one directory
	 * called "a" ends up relocated into rr_moved. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae,
	    "a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/f.txt");
	archive_entry_set_size(ae, 4);
	archive_entry_set_mode(ae, S_IFREG | 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 4, archive_write_data(a, "body", 4));
	archive_entry_free(ae);

	/* The second "a" cannot be inserted into rr_moved; the close
	 * reports the error instead of crashing. */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_write_close(a));

	/* Freeing the archive must release the relocated subtree that
	 * the failed insertion had taken away from its parent. */
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	free(buff);
}

/*
 * Deep trees whose directory names are unique are relocated into
 * rr_moved successfully.
 */
DEFINE_TEST(test_write_format_iso9660_rr_moved_unique)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buff;
	size_t buffsize = 256 * 2048;
	size_t used;

	buff = malloc(buffsize);
	assert(buff != NULL);
	if (buff == NULL)
		return;

	assert((a = archive_write_new()) != NULL);
	assertA(0 == archive_write_set_format_iso9660(a));
	assertA(0 == archive_write_add_filter_none(a));
	assertA(0 == archive_write_set_bytes_per_block(a, 1));
	assertA(0 == archive_write_set_bytes_in_last_block(a, 1));
	assertA(0 == archive_write_open_memory(a, buff, buffsize, &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae,
	    "d1/d2/d3/d4/d5/d6/d7/d8/d9/d10/d11/d12/d13/file.txt");
	archive_entry_set_size(ae, 4);
	archive_entry_set_mode(ae, S_IFREG | 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 4, archive_write_data(a, "body", 4));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));

	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	free(buff);
}
