/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test that entries with path traversal names are rejected */
DEFINE_TEST(test_insecure_paths)
{
#ifdef HAVE_LIBZ
	const char *reffile = "test_insecure_paths.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);

	/* The one safe entry must be extracted */
	assertTextFileContents("safe\n", "safe.txt");

	/* Insecure entries must be skipped; warnings go to stderr */
	assertNonEmptyFile("test.err");

	/* None of the traversal targets must exist */
	assertFileNotExists("../escape.txt");
	assertFileNotExists("escape2.txt");
	assertFileNotExists("absolute.txt");
#else
	skipping("zlib not available");
#endif
}
