/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Mostyn Bramley-Moore <mostyn@antipode.se>
 */

#include "test.h"

#include <stdlib.h>

#if defined(_WIN32) && !defined(__CYGWIN__)

#include <fileapi.h>

#define UNLINK _unlink

#else

#define UNLINK unlink

#endif

DEFINE_TEST(test_extract_cpio_absolute_paths)
{
	int r;

#if defined(_WIN32) && !defined(__CYGWIN__)
	char temp_dir[MAX_PATH + 1];
	assert(GetTempPathA(MAX_PATH + 1, temp_dir) != 0);

	char temp_file_name[MAX_PATH + 1];
	char temp_absolute_file_name[MAX_PATH + 1];
	assert(GetTempFileNameA(temp_dir, "abs", 0, temp_file_name) != 0);

	assert(_fullpath(temp_absolute_file_name, temp_file_name, MAX_PATH) != NULL);
#else
	char temp_absolute_file_name[] = "/tmp/cpio-noabs.testXXXXXX";
	mkstemp(temp_absolute_file_name);
#endif

	UNLINK(temp_absolute_file_name);

	const char *sample_data = "test file from test_extract_cpio_absolute_paths";

	assertMakeFile("filelist", 0644, temp_absolute_file_name);
	assertMakeFile(temp_absolute_file_name, 0644, sample_data);

	// Create an archive with the absolute path.
	r = systemf("%s -o < filelist > archive.cpio 2> stderr1.txt", testprog);
	UNLINK("filelist");
	assertEqualInt(r, 0);

	// Ensure that the temp file does not exist.
	UNLINK(temp_absolute_file_name);

	// We should refuse to create the absolute path without --insecure.
	r = systemf("%s -i < archive.cpio 2> stderr2.txt", testprog);
	//assert(r != 0); // Should this command fail?
	assertFileNotExists(temp_absolute_file_name);
	UNLINK(temp_absolute_file_name); // Cleanup just in case.

	// But if we specify --insecure then the absolute path should be created.
	r = systemf("%s -i --insecure < archive.cpio 2> stderr3.txt", testprog);
	assert(r == 0);
	assertFileExists(temp_absolute_file_name);
	UNLINK(temp_absolute_file_name);
}
