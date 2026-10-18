/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Tim Kientzle
 * Copyright (c) 2026 Eduard Zateev
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_missing_file)
{
	int r;

	assertMakeFile("file1", 0644, "file1");
	assertMakeFile("file2", 0644, "file2");

	assertMakeFile("filelist1", 0644, "file1\nfile2\n");
	r = systemf("%s -o <filelist1 >stdout1 2>stderr1", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "stderr1");

	assertMakeFile("filelist2", 0644, "file1\nfile2\nfile3\n");
	r = systemf("%s -o <filelist2 >stdout2 2>stderr2", testprog);
	assert(r != 0);

	assertMakeFile("filelist3", 0644, "");
	r = systemf("%s -o <filelist3 >stdout3 2>stderr3", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "stderr3");

	assertMakeFile("filelist4", 0644, "file3\n");
	r = systemf("%s -o <filelist4 >stdout4 2>stderr4", testprog);
	assert(r != 0);
}


DEFINE_TEST(test_header_failure_exit_status)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	skipping("requires creating a sparse file larger than 4 GiB");
#else
	int fd, r;
	off_t size = (off_t)UINT64_C(4294967296);

	if ((uint64_t)size != UINT64_C(4294967296)) {
		skipping("off_t cannot represent a file larger than 4 GiB");
		return;
	}

	fd = open("large.img", O_CREAT | O_WRONLY | O_BINARY, 0644);
	if (fd < 0) {
		skipping("could not create sparse test file");
		return;
	}
	if (ftruncate(fd, size) != 0) {
		close(fd);
		skipping("could not create sparse test file");
		return;
	}
	close(fd);

	assertMakeFile("filelist", 0644, "large.img\n");
	r = systemf("%s -o -H newc <filelist >out 2>err", testprog);
	assertEqualInt(r, 1);
#endif
}
