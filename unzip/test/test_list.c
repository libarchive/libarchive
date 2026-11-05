/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 libarchive Contributors
 * All rights reserved.
 */
#include "test.h"

/*
 * The per-entry lines contain local timestamps, so only check that
 * the listing ends with the expected summary footer.
 */
static void
assert_listing_footer(const char *footer)
{
	char *p;
	size_t s, flen;

	flen = strlen(footer);
	p = slurpfile(&s, "test.out");
	assert(p != NULL);
	if (p == NULL)
		return;
	assert(s >= flen);
	if (s >= flen)
		assertEqualString(footer, p + s - flen);
	free(p);
}

/* Test l arg - List files with totals */
DEFINE_TEST(test_list_l)
{
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -l %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");
	assert_listing_footer(
	    " --------                   -------\n"
	    "       47                   5 files\n");
}

/* Test v arg - Verbose listing with totals */
DEFINE_TEST(test_list_v)
{
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -v %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");
	assert_listing_footer(
	    "--------          -------  ---                            -------\n"
	    "      47               47   0%                            5 files\n");
}
