/*-SPDX-License-Identifier: BSD-2-Clause
 * Copyright © 2025 ARJANEN Loïc Jean David
 * All rights reserved.
 */

 #include "test.h"

 /*
  * These tests verify using our reader that we can use NTFS extra data
  * to store high-resolution times if requested as well as to store
  * beyond-2038 times.
  */

DEFINE_TEST(test_write_format_zip_high_resolution_time)
{
	struct archive_entry *ae;
	struct archive *a;
	size_t used;
	char buff[1000000];
	char filedata[9999];
 
	/*
	 * First write an archive with high-resolution times requested,
	 * both files should have nanosecond times set.
	 */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "zip:compression=store"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "zip:high-resolution-time=yes"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff), &used));

	/*
	 * Write the first file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 999, 1000);
	archive_entry_copy_pathname(ae, "file1");
	archive_entry_set_mode(ae, AE_IFREG | 0755);
	archive_entry_set_size(ae, 10);
	assertEqualInt(0, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(10, archive_write_data(a, "0123456789", 10));

	/*
	 * Write the second file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_ctime(ae, 10, 900);
	archive_entry_copy_pathname(ae, "file2");
	archive_entry_set_mode(ae, AE_IFREG | 0755);
	archive_entry_set_size(ae, 15);
	assertEqualInt(0, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(15, archive_write_data(a, "&\"'(-_=)[]|{}@#", 15));

	/* Close out the archive. */
	assertEqualInt(ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * Now, read the data back with the standard memory reader.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));

	/* Read and verify first file. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(999, archive_entry_mtime(ae));
	/* We requested it so should have high-resolution mtime. */
	assertEqualInt(1000, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(10, archive_entry_size(ae));
	assert(archive_entry_size_is_set(ae));
	assertEqualIntA(a, 10,
		 archive_read_data(a, filedata, sizeof(filedata)));
	assertEqualMem(filedata, "0123456789", 10);

	/* Read the second file back. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(0, archive_entry_mtime(ae));
	/* We requested high-resolution ctime this time. */
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(10, archive_entry_ctime(ae));
	assertEqualInt(900, archive_entry_ctime_nsec(ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(15, archive_entry_size(ae));
	assert(archive_entry_size_is_set(ae));
	assertEqualIntA(a, 15,
		archive_read_data(a, filedata, sizeof(filedata)));
	assertEqualMem(filedata, "&\"'(-_=)[]|{}@#", 15);

	/* Close out the archive. */
	assertEqualInt(ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
 
	/*
	 * Second write an archive with high-resolution times not requested,
	 * only the second file should have nanosecond times set being beyond
	 * 2038.
	 */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "zip:compression=store"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_option(a, "zip", "high-resolution-time", NULL));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff), &used));

	/*
	 * Write the first file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 12586, 120);
	archive_entry_copy_pathname(ae, "file1");
	archive_entry_set_mode(ae, AE_IFREG | 0755);
	archive_entry_set_size(ae, 10);
	assertEqualInt(0, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(10, archive_write_data(a, "0123456789", 10));

	/*
	 * Write the second file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_ctime(ae, 0x100000000, 9000);
	archive_entry_copy_pathname(ae, "file2");
	archive_entry_set_mode(ae, AE_IFREG | 0755);
	archive_entry_set_size(ae, 15);
	assertEqualInt(0, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(15, archive_write_data(a, "&\"'(-_=)[]|{}@#", 15));

	/* Close out the archive. */
	assertEqualInt(ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * Now, read the data back with the standard memory reader.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));

	/* Read and verify first file. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(12586, archive_entry_mtime(ae));
	/* We didn't request high-resolution times, so shouldn't have mtime's nsecs. */
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(10, archive_entry_size(ae));
	assert(archive_entry_size_is_set(ae));
	assertEqualIntA(a, 10,
		 archive_read_data(a, filedata, sizeof(filedata)));
	assertEqualMem(filedata, "0123456789", 10);

	/* Read the second file back. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(0, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0x100000000, archive_entry_ctime(ae));
	/* Time is beyond 2038, so we should have high-resolution ctime regardless. */
	assertEqualInt(9000, archive_entry_ctime_nsec(ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(15, archive_entry_size(ae));
	assert(archive_entry_size_is_set(ae));
	assertEqualIntA(a, 15,
		archive_read_data(a, filedata, sizeof(filedata)));
	assertEqualMem(filedata, "&\"'(-_=)[]|{}@#", 15);

	/* Close out the archive. */
	assertEqualInt(ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}