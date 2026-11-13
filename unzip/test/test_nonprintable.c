/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Socialpranker
 * All rights reserved.
 */
#include "test.h"

/*
 * Entry names come from the archive and may hold terminal control
 * sequences.  They must be printed escaped, the way bsdtar prints
 * them, and not as raw bytes.
 *
 * The reference archive holds one stored file "esc<ESC>[1Aname".
 */

static void
assertFileHasEscapedName(const char *path)
{
	char *p;
	size_t s;

	p = slurpfile(&s, "%s", path);
	assert(p != NULL);
	if (p == NULL)
		return;
	assert(strstr(p, "esc\\033[1Aname") != NULL);
	assert(memchr(p, '\033', s) == NULL);
	free(p);
}

DEFINE_TEST(test_nonprintable)
{
	const char *reffile = "test_nonprintable.zip";
	int r;

	extract_reference_file(reffile);

	r = systemf("%s -Z1 %s >Z1.out 2>Z1.err", testprog, reffile);
	assertEqualInt(0, r);
	assertTextFileContents("esc\\033[1Aname\n", "Z1.out");
	assertEmptyFile("Z1.err");

	r = systemf("%s -l %s >l.out 2>l.err", testprog, reffile);
	assertEqualInt(0, r);
	assertFileHasEscapedName("l.out");

	r = systemf("%s -v %s >v.out 2>v.err", testprog, reffile);
	assertEqualInt(0, r);
	assertFileHasEscapedName("v.out");

	r = systemf("%s -t %s >t.out 2>t.err", testprog, reffile);
	assertEqualInt(0, r);
	assertFileHasEscapedName("t.out");

#if !defined(_WIN32) || defined(__CYGWIN__)
	r = systemf("%s %s >x.out 2>x.err", testprog, reffile);
	assertEqualInt(0, r);
	assertFileHasEscapedName("x.out");
#endif
}
