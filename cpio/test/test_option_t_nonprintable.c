/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Socialpranker
 * All rights reserved.
 */
#include "test.h"

/*
 * Entry names and link targets come from the archive and may hold
 * terminal control sequences.  They must be printed escaped, the way
 * bsdtar prints them, and not as raw bytes.
 *
 * The reference archive holds a file "esc<ESC>[1Aname" and a symlink
 * "link" whose target is "tgt<CR>x".
 */

static void
assertNoControlBytes(const char *p)
{
	assert(strchr(p, '\033') == NULL);
	assert(strchr(p, '\r') == NULL);
}

DEFINE_TEST(test_option_t_nonprintable)
{
	const char *reffile = "test_option_t_nonprintable.cpio";
	char *p;
	int r;

	extract_reference_file(reffile);

	/* Plain listing. */
	r = systemf("%s -it < %s >it.out 2>it.err", testprog, reffile);
	assertEqualInt(r, 0);
	assertTextFileContents("esc\\033[1Aname\nlink\n", "it.out");

	/* Verbose listing, including the symlink target. */
	r = systemf("%s -itv < %s >itv.out 2>itv.err", testprog, reffile);
	assertEqualInt(r, 0);
	p = slurpfile(NULL, "itv.out");
	assert(p != NULL);
	if (p != NULL) {
		assert(strstr(p, " esc\\033[1Aname\n") != NULL);
		assert(strstr(p, " link -> tgt\\rx\n") != NULL);
		assertNoControlBytes(p);
		free(p);
	}

#if !defined(_WIN32) || defined(__CYGWIN__)
	/* Verbose extraction prints the names on stderr. */
	r = systemf("%s -idv < %s >idv.out 2>idv.err", testprog, reffile);
	assertEqualInt(r, 0);
	p = slurpfile(NULL, "idv.err");
	assert(p != NULL);
	if (p != NULL) {
		assert(strstr(p, "esc\\033[1Aname\n") != NULL);
		assertNoControlBytes(p);
		free(p);
	}
#endif
}
