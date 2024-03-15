/*
 * Copyright (c) 2003-2018
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

DEFINE_TEST(test_7zip_filename_encoding_UTF16_win)
{
#if !defined(_WIN32) || defined(__CYGWIN__)
	skipping("This test is meant to verify unicode string handling"
		" on Windows with UTF-16 names");
	return;
#else
	struct archive *a;
	struct archive_entry *entry;
	char buff[4096];
	size_t used;

	/*
	 * Don't call setlocale because we're verifying that the '_w' functions
	 * work as expected
	 */

	/* Part 1: file */
	a = archive_write_new();
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_7zip(a));
	assertEqualInt(ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	entry = archive_entry_new2(a);
	/* Set the filename using a UTF-16 string */
	archive_entry_copy_pathname_w(entry, L"\u8868.txt");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Check UTF-16 version. */
	assertEqualMem(buff + 44, L"\u8868.txt", 10);

	/* Part 2: directory */
	a = archive_write_new();
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_7zip(a));
	assertEqualInt(ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	entry = archive_entry_new2(a);
	/* Set the directory name using a UTF-16 string */
	/* NOTE: Explicitly not adding trailing slash to test that code path */
	archive_entry_copy_pathname_w(entry, L"\u8868");
	archive_entry_set_filetype(entry, AE_IFDIR);
	archive_entry_set_size(entry, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* NOTE: Directories do not get trailing slash for 7zip files */
	assertEqualMem(buff + 41, L"\u8868", 2);

	/* Part 3: symlink */
	a = archive_write_new();
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_7zip(a));
	assertEqualInt(ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	entry = archive_entry_new2(a);
	/* Set the symlink target using a UTF-16 string */
	archive_entry_set_pathname(entry, "link.txt");
	archive_entry_copy_symlink_w(entry, L"\u8868.txt");
	archive_entry_set_filetype(entry, AE_IFLNK);
	archive_entry_set_symlink_type(entry, AE_SYMLINK_TYPE_FILE);
	archive_entry_set_size(entry, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* NOTE: link names are stored in UTF-8 */
	assertEqualMem(buff + 32, "\xE8\xA1\xA8.txt", 7);

	/* NOTE: 7zip does not support hardlinks */
#endif
}
