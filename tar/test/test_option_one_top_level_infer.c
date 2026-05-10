/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Michihiro NAKAJIMA  (as test_extract_tar_Z.c)
 *               2026 dzwdz
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_one_top_level_infer)
{
	const char *reffile = "test_extract.tar.Z";

	extract_reference_file(reffile);
	assertEqualInt(0, systemf("%s -xf %s --one-top-level "
	    ">test.out 2>test.err", testprog, reffile));

	assertFileExists("test_extract/file1");;
	assertTextFileContents("contents of file1.\n", "test_extract/file1");
	assertFileExists("test_extract/file2");
	assertTextFileContents("contents of file2.\n", "test_extract/file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
}
