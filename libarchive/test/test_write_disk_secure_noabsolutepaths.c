/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Mostyn Bramley-Moore <mostyn@antipode.se>
 */

#include "test.h"

#if defined(_WIN32) && !defined(__CYGWIN__)

#include <fileapi.h>

#define UNLINK _unlink

#else

#include <stdlib.h>
#include <unistd.h>

#define UNLINK unlink

#endif

/*
 * Exercise security checks that should prevent writing absolute paths
 * when extracting archives.
 */
DEFINE_TEST(test_write_disk_secure_noabsolutepaths)
{
	struct archive *a, *ad;
	struct archive_entry *ae;

	char buff[10000];

	size_t used;

	// Create an archive_write object.
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff), &used));

#if defined(_WIN32) && !defined(__CYGWIN__)
	char temp_dir[MAX_PATH + 1];
	assert(GetTempPathA(MAX_PATH + 1, temp_dir) != 0);

	char temp_file_name[MAX_PATH + 1];
	char temp_absolute_file_name[MAX_PATH + 1];
	assert(GetTempFileNameA(temp_dir, "abs", 0, temp_file_name) != 0);

	assert(_fullpath(temp_absolute_file_name, temp_file_name, MAX_PATH) != NULL);

	// Convert to a unix-style path.
	for (char *p = temp_absolute_file_name; *p != '\0'; p++)
		if (*p == '\\') *p = '/';

#else
	char temp_absolute_file_name[] = "/tmp/noabs.testXXXXXX";
	mkstemp(temp_absolute_file_name);
#endif

	// Ensure that the target file does not exist.
	UNLINK(temp_absolute_file_name);

	// Add a regular file entry with an absolute path.
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, temp_absolute_file_name);
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_size(ae, 6);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(6, archive_write_data(a, "hello", 6));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	// Now try to extract the data.
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(temp_absolute_file_name, archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(AE_IFREG | 0777, archive_entry_mode(ae));
	assertEqualInt(6, archive_entry_size(ae));

	// This should succeed.
	assertEqualInt(ARCHIVE_OK, archive_read_extract(a, ae, 0));
	UNLINK(temp_absolute_file_name);

	// This should fail, since the archive entry has an absolute path.
	assert(ARCHIVE_OK != archive_read_extract(a, ae, ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS));

	// This should also fail.
	assert((ad = archive_write_new()) != NULL);
	assertEqualInt(ARCHIVE_OK, archive_write_disk_set_options(a, ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS));
	assert(ARCHIVE_OK != archive_read_extract2(a, ae, ad));

	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
