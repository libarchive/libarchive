/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Akash Deshmukh
 * All rights reserved.
 */
#include "test.h"

/* Test that a long answer to the overwrite prompt is read as one line */
DEFINE_TEST(test_prompt_long_answer)
{
#ifdef HAVE_LIBZ
	const char *reffile = "test_basic.zip";
	int r;

	assertMakeDir("test_basic", 0755);
	assertMakeFile("test_basic/a", 0644, "orig a\n");
	assertMakeFile("test_basic/b", 0644, "orig b\n");
	assertMakeFile("answers", 0644, "yes\nno\n");

	extract_reference_file(reffile);
	r = systemf("%s %s <answers >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertTextFileContents(
	    "replace test_basic/a? [y]es, [n]o, [A]ll, [N]one, [r]ename: "
	    "replace test_basic/b? [y]es, [n]o, [A]ll, [N]one, [r]ename: ",
	    "test.err");

	assertTextFileContents("contents a\n", "test_basic/a");
	assertTextFileContents("orig b\n", "test_basic/b");
	assertTextFileContents("contents c\n", "test_basic/c");
	assertTextFileContents("contents CAPS\n", "test_basic/CAPS");
#else
	skipping("zlib not available");
#endif
}
