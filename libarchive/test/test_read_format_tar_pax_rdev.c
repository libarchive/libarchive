/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Wleeaf
 * All rights reserved.
 */
#include "test.h"

/*
 * The pax writer stores a device number of 2^18 or more in a
 * SCHILY.devmajor or SCHILY.devminor attribute, and the other number
 * only in the ustar header.  Both numbers must be read back.
 */
DEFINE_TEST(test_read_format_tar_pax_rdev)
{
	static const struct {
		dev_t major;
		dev_t minor;
	} devs[] = {
		{ 8, 300000 },
		{ 300000, 8 },
		{ 300000, 300000 },
		{ 8, 1 },
	};
	char buff[16384];
	struct archive_entry *ae;
	struct archive *a;
	size_t used, i;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_pax(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	for (i = 0; i < sizeof(devs) / sizeof(devs[0]); i++) {
		assert((ae = archive_entry_new()) != NULL);
		archive_entry_set_pathname(ae, "dev");
		archive_entry_set_filetype(ae, AE_IFCHR);
		archive_entry_set_perm(ae, 0644);
		archive_entry_set_rdevmajor(ae, devs[i].major);
		archive_entry_set_rdevminor(ae, devs[i].minor);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		archive_entry_free(ae);
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	for (i = 0; i < sizeof(devs) / sizeof(devs[0]); i++) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualInt(AE_IFCHR, archive_entry_filetype(ae));
		assertEqualInt(devs[i].major, archive_entry_rdevmajor(ae));
		assertEqualInt(devs[i].minor, archive_entry_rdevminor(ae));
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
