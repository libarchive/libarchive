/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Michihiro NAKAJIMA  (as test_extract_tar_Z.c)
 *               2026 dzwdz
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_one_top_level)
{
	const char *reffile = "test_extract.tar.Z";

	extract_reference_file(reffile);
	assertEqualInt(0, systemf("%s -xf %s --one-top-level=meow "
	    ">test.out 2>test.err", testprog, reffile));

	assertFileExists("meow/file1");;
	assertTextFileContents("contents of file1.\n", "meow/file1");
	assertFileExists("meow/file2");
	assertTextFileContents("contents of file2.\n", "meow/file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
}
