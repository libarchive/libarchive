/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Martin Mirchev
 * All rights reserved.
 */
#include "test.h"

/*
 * A user-supplied rr_moved directory is moved to the head of the root's
 * children before deep directories are relocated into it.  When the
 * root also holds a regular file that sorts first, rr_moved is not the
 * first child but it is the first sub-directory; removing it from the
 * sub-directory list must still work.
 */
DEFINE_TEST(test_write_format_iso9660_rr_moved_order)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buff;
	size_t buffsize = 256 * 2048;
	size_t used;
	int found = 0;

	buff = malloc(buffsize);
	assert(buff != NULL);
	if (buff == NULL)
		return;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_iso9660(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_per_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_in_last_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	/* A regular file at the root, before rr_moved. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "a.txt");
	archive_entry_set_size(ae, 2);
	archive_entry_set_mode(ae, S_IFREG | 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 2, archive_write_data(a, "a\n", 2));
	archive_entry_free(ae);

	/* A directory called rr_moved supplied by the caller. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "rr_moved");
	archive_entry_set_size(ae, 0);
	archive_entry_set_mode(ae, S_IFDIR | 0755);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* A path nested more than eight levels deep. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae,
	    "d1/d2/d3/d4/d5/d6/d7/d8/d9/file.txt");
	archive_entry_set_size(ae, 4);
	archive_entry_set_mode(ae, S_IFREG | 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 4, archive_write_data(a, "body", 4));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* The deep file is still found under its original path. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
		if (strcmp(archive_entry_pathname(ae),
		    "d1/d2/d3/d4/d5/d6/d7/d8/d9/file.txt") == 0)
			found = 1;
	}
	assert(found);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
}
