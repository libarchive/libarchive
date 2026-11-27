/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/*
 * A header with a bad checksum after the first entry causes the
 * tar reader to return ARCHIVE_RETRY and skip ahead to the next
 * valid header.  bsdtar keeps going, but it must still report
 * the damage in its exit status.  (Issue #3900)
 */
DEFINE_TEST(test_bad_header_checksum)
{
	const char *refname = "test_bad_header_checksum.tar";
	int r;

	extract_reference_file(refname);

	/* Extract */
	r = systemf("%s -xf %s >extract.out 2>extract.err",
	    testprog, refname);
	failure("A skipped damaged header must be reported in the exit status");
	assert(r != 0);
	assertEmptyFile("extract.out");
	assertNonEmptyFile("extract.err");
	/* The entries around the damaged one are still extracted. */
	assertFileContents("a\n", 2, "a");
	assertFileNotExists("b");
	assertFileContents("c\n", 2, "c");

	/* List */
	r = systemf("%s -tf %s >list.out 2>list.err",
	    testprog, refname);
	failure("A skipped damaged header must be reported in the exit status");
	assert(r != 0);
	assertTextFileContents("a\nc\n", "list.out");
	assertNonEmptyFile("list.err");
}
