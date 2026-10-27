/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Junker der Provinz
 * All rights reserved.
 */
#include "test.h"

/*
 * An archive written to a regular file must refuse an entry for
 * that same file, as read from disk, and accept any other file.
 */
DEFINE_TEST(test_write_open_filename_skip_self)
{
	struct archive *a, *disk;
	struct archive_entry *entry;

	assertMakeFile("other", 0644, "");

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_filename(a, "out.tar"));

	assert((disk = archive_read_disk_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);

	archive_entry_copy_pathname(entry, "out.tar");
	assertEqualIntA(disk, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(disk, entry, -1, NULL));
	assertEqualIntA(a, ARCHIVE_FAILED, archive_write_header(a, entry));
	assertEqualString("Can't add archive to itself",
	    archive_error_string(a));

	archive_entry_clear(entry);
	archive_entry_copy_pathname(entry, "other");
	assertEqualIntA(disk, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(disk, entry, -1, NULL));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));

	archive_entry_free(entry);
	assertEqualInt(ARCHIVE_OK, archive_read_free(disk));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}
