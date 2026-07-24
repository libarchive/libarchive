/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Dan Nelson
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_mode)
{
	int rv;
	char *p;

	assertMakeDir("in", 0755);

	/* Test invalid modes */
	rv = systemf("%s --mode 8 -cf archive.tar in > 1.out 2> 1.err",
		testprog);
	assert(rv != 0);

	rv = systemf("%s --mode a+a -cf archive.tar in > 2.out 2> 2.err",
		testprog);
	assert(rv != 0);

	/* Create some files with different modes */
	assertMakeFile("in/all", 0777, "");
	assertMakeFile("in/minimal", 0500, "");

	/* Archive and override using an absolute mode */
	assertEqualInt(0,
		systemf("%s --mode 644 -cf archive1.tar "
			"in/all in/minimal", testprog));

	/* Verify the modes */
	p = slurpfile(NULL, "archive1.tar");
	assertEqualString(p + 100,"000644 ");
	assertEqualString(p + 612,"000644 ");
	free(p);

/* Skip relative symbolic mode checks on Windows; on-disk files always have
 * mode 644.
 */
#if !defined(_WIN32) || defined(__CYGWIN__)
	/* Archive and override using a symbolic mode */
	assertEqualInt(0,
		systemf("%s --mode u+rw-x,g+X,o-w -cf archive2.tar "
			"in/all in/minimal", testprog));

	/* Verify the modes */
	p = slurpfile(NULL, "archive2.tar");
	assertEqualString(p + 100,"000675 ");
	assertEqualString(p + 612,"000610 ");
	free(p);
#endif

}
