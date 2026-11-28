/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 François Degros
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_stdin_name)
{
	if (!canGzip()) {
		skipping("gzip is not supported on this platform");
		return;
	}

	assertMakeFile("file", 0644, "hello\n");
	assertEqualInt(0, systemf("%s -cf a.tar file", testprog));
	assertEqualInt(0, systemf("%s -czf a.tgz file", testprog));

	/* The name given for standard input tells how to read it. */
	assertEqualInt(0, systemf("%s --trust-extension --stdin-name backup.tgz "
	    "-tf - <a.tgz >test1.out 2>test1.err", testprog));
	assertTextFileContents("file\n", "test1.out");
	assertEmptyFile("test1.err");

	/* It is also the default archive. */
	assertEqualInt(0, systemf("%s --trust-extension --stdin-name backup.tgz "
	    "-t <a.tgz >test2.out 2>test2.err", testprog));
	assertTextFileContents("file\n", "test2.out");
	assertEmptyFile("test2.err");

	assertMakeDir("extract", 0755);
	assertChdir("extract");
	assertEqualInt(0, systemf("%s --trust-extension --stdin-name a.tgz "
	    "-xf - <../a.tgz >test.out 2>test.err", testprog));
	assertTextFileContents("hello\n", "file");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* A compressed file is named after the name that was given. */
	assertMakeFile("data.txt", 0644, "some text\n");
	assertEqualInt(0, systemf("gzip -c <data.txt >data.txt.gz"));
	assertEqualInt(0, systemf("%s --trust-extension --stdin-name dir/other.txt.gz "
	    "-tf - <data.txt.gz >test3.out 2>test3.err", testprog));
	assertTextFileContents("other.txt\n", "test3.out");
	assertEmptyFile("test3.err");

	/* The name is only a hint for --trust-extension. */
	assert(0 != systemf("%s --stdin-name a.tar -tf - <a.tar "
	    ">test4.out 2>test4.err", testprog));
	assertEmptyFile("test4.out");
	assertNonEmptyFile("test4.err");

	/* It is for standard input only. */
	assert(0 != systemf("%s --trust-extension --stdin-name a.tar -tf a.tar "
	    ">test5.out 2>test5.err", testprog));
	assertEmptyFile("test5.out");
	assertNonEmptyFile("test5.err");

	/* An unrecognized name is an error, as with a real file. */
	assert(0 != systemf("%s --trust-extension --stdin-name a.unknown -tf - "
	    "<a.tar >test6.out 2>test6.err", testprog));
	assertEmptyFile("test6.out");
	assertNonEmptyFile("test6.err");

	/* Standard input has no name unless one is given. */
	assert(0 != systemf("%s --trust-extension -tf - <a.tar "
	    ">test7.out 2>test7.err", testprog));
	assertEmptyFile("test7.out");
	assertNonEmptyFile("test7.err");

	/* It is for reading archives only. */
	assert(0 != systemf("%s --trust-extension --stdin-name a.tar -cf b.tar "
	    "file >test8.out 2>test8.err", testprog));
	assertNonEmptyFile("test8.err");
}
