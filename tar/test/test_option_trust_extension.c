/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 François Degros
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_trust_extension)
{
	if (!canGzip()) {
		skipping("gzip is not supported on this platform");
		return;
	}

	assertMakeFile("file", 0644, "hello\n");
	assertEqualInt(0, systemf("%s -cf a.tar file", testprog));
	assertEqualInt(0, systemf("%s -czf a.tgz file", testprog));

	/* A name that is recognized is trusted. */
	assertEqualInt(0, systemf("%s --trust-extension -tf a.tar "
	    ">test1.out 2>test1.err", testprog));
	assertTextFileContents("file\n", "test1.out");
	assertEmptyFile("test1.err");

	assertEqualInt(0, systemf("%s --trust-extension -tf a.tgz "
	    ">test2.out 2>test2.err", testprog));
	assertTextFileContents("file\n", "test2.out");
	assertEmptyFile("test2.err");

	assertMakeDir("extract", 0755);
	assertChdir("extract");
	assertEqualInt(0, systemf("%s --trust-extension -xf ../a.tgz "
	    ">test.out 2>test.err", testprog));
	assertTextFileContents("hello\n", "file");
	assertEmptyFile("test.err");
	assertChdir("..");

	/*
	 * A name that doesn't match the contents is not second-guessed, though
	 * the archive can be read without the option.
	 */
	assertEqualInt(0, systemf("cp a.tgz mislabeled.tar"));
	assertEqualInt(0, systemf("%s -tf mislabeled.tar >test3.out 2>test3.err",
	    testprog));
	assertTextFileContents("file\n", "test3.out");
	assert(0 != systemf("%s --trust-extension -tf mislabeled.tar "
	    ">test4.out 2>test4.err", testprog));
	assertEmptyFile("test4.out");
	assertNonEmptyFile("test4.err");

	/* A name that is not recognized is an error, not a guess. */
	assertEqualInt(0, systemf("cp a.tar archive.unknown"));
	assertEqualInt(0, systemf("%s -tf archive.unknown "
	    ">test5.out 2>test5.err", testprog));
	assertTextFileContents("file\n", "test5.out");
	assert(0 != systemf("%s --trust-extension -tf archive.unknown "
	    ">test6.out 2>test6.err", testprog));
	assertEmptyFile("test6.out");
	assertNonEmptyFile("test6.err");

	/* Standard input has no name. */
	assert(0 != systemf("%s --trust-extension -tf - <a.tar "
	    ">test7.out 2>test7.err", testprog));
	assertEmptyFile("test7.out");
	assertNonEmptyFile("test7.err");

	/* It is for reading archives only. */
	assert(0 != systemf("%s --trust-extension -cf b.tar file "
	    ">test8.out 2>test8.err", testprog));
	assertNonEmptyFile("test8.err");
}
