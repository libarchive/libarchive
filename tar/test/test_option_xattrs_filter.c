/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Nikolay Bryskin
 * All rights reserved.
 */
#include "test.h"

/*
 * Exercise --xattrs-exclude / --xattrs-include name filtering on both the
 * create (read-disk) and extract (write-disk) sides.  Two user.* attributes
 * are used so the test needs no special privileges; the filtering code path
 * is namespace-agnostic (the motivating case is dropping security.selinux
 * while keeping security.capability).
 */
DEFINE_TEST(test_option_xattrs_filter)
{
#if !ARCHIVE_XATTR_SUPPORT
	skipping("Extended attributes are not supported on this platform");
#else	/* ARCHIVE_XATTR_SUPPORT */
	const char *keep = "user.libarchive.keep";
	const char *drop = "user.libarchive.drop";
	const char *val = "v";
	void *readval;
	size_t size;
	int r;

	assertMakeFile("f", 0644, "a");
	if (!setXattr("f", keep, val, strlen(val) + 1) ||
	    !setXattr("f", drop, val, strlen(val) + 1)) {
		skipping("Can't set user extended attributes on this "
		    "filesystem");
		return;
	}

	/* Create side: --xattrs-exclude drops just the matching name. */
	r = systemf("%s -c --no-mac-metadata --xattrs "
	    "--xattrs-exclude %s -f excl.tar f >excl.out 2>excl.err",
	    testprog, drop);
	assertEqualInt(r, 0);
	assertMakeDir("excl", 0755);
	r = systemf("%s -x -C excl --no-same-permissions --xattrs "
	    "-f excl.tar >excl_x.out 2>excl_x.err", testprog);
	assertEqualInt(r, 0);
	readval = getXattr("excl/f", keep, &size);
	if (assertEqualInt(size, strlen(val) + 1) != 0)
		assertEqualMem(readval, val, size);
	free(readval);
	assert(getXattr("excl/f", drop, &size) == NULL);

	/* Create side: --xattrs-include keeps only the matching name. */
	r = systemf("%s -c --no-mac-metadata --xattrs "
	    "--xattrs-include %s -f incl.tar f >incl.out 2>incl.err",
	    testprog, keep);
	assertEqualInt(r, 0);
	assertMakeDir("incl", 0755);
	r = systemf("%s -x -C incl --no-same-permissions --xattrs "
	    "-f incl.tar >incl_x.out 2>incl_x.err", testprog);
	assertEqualInt(r, 0);
	readval = getXattr("incl/f", keep, &size);
	if (assertEqualInt(size, strlen(val) + 1) != 0)
		assertEqualMem(readval, val, size);
	free(readval);
	assert(getXattr("incl/f", drop, &size) == NULL);

	/* Extract side: archive carries both, extraction drops one. */
	r = systemf("%s -c --no-mac-metadata --xattrs -f both.tar f "
	    ">both.out 2>both.err", testprog);
	assertEqualInt(r, 0);
	assertMakeDir("xexcl", 0755);
	r = systemf("%s -x -C xexcl --no-same-permissions --xattrs "
	    "--xattrs-exclude %s -f both.tar >xexcl.out 2>xexcl.err",
	    testprog, drop);
	assertEqualInt(r, 0);
	readval = getXattr("xexcl/f", keep, &size);
	if (assertEqualInt(size, strlen(val) + 1) != 0)
		assertEqualMem(readval, val, size);
	free(readval);
	assert(getXattr("xexcl/f", drop, &size) == NULL);
#endif	/* ARCHIVE_XATTR_SUPPORT */
}
