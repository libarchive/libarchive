/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_exclude)
{
	int r;

	assertMakeFile("file1", 0644, "file1");
	assertMakeFile("file2", 0644, "file2");
	assertEqualInt(0, systemf("%s -cf archive.tar file1 file2", testprog));

	/*
	 * Now, try extracting from the test archive with various --exclude options.
	 */

	/* Test 1: Without --exclude */
	assertMakeDir("test1", 0755);
	assertChdir("test1");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar >test.out 2>test.err", testprog));
	assertFileContents("file1", 5, "file1");
	assertFileContents("file2", 5, "file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 2: Selecting just one file */
	assertMakeDir("test2", 0755);
	assertChdir("test2");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar file1 >test.out 2>test.err", testprog));
	assertFileContents("file1", 5, "file1");
	assertFileNotExists("file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 3: Use --exclude to skip one file */
	assertMakeDir("test3", 0755);
	assertChdir("test3");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar --exclude file1 >test.out 2>test.err", testprog));
	assertFileNotExists("file1");
	assertFileContents("file2", 5, "file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 4: Selecting one valid and one invalid file */
	assertMakeDir("test4", 0755);
	assertChdir("test4");
	r = systemf("%s -xf ../archive.tar file1 file3 >test.out 2>test.err", testprog);
	assert(r != 0);
	assertFileContents("file1", 5, "file1");
	assertFileNotExists("file2");
	assertFileNotExists("file3");
	assertEmptyFile("test.out");
	assertNonEmptyFile("test.err");
	assertChdir("..");

	/* Test 5: Selecting one valid file twice */
	assertMakeDir("test5", 0755);
	assertChdir("test5");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar file1 file1 >test.out 2>test.err", testprog));
	assertFileContents("file1", 5, "file1");
	assertFileNotExists("file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 6: Include and exclude the same file */
	assertMakeDir("test6", 0755);
	assertChdir("test6");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar --exclude file1 file1 >test.out 2>test.err", testprog));
	assertFileNotExists("file1");
	assertFileNotExists("file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 7: Exclude a non-existent file */
	assertMakeDir("test7", 0755);
	assertChdir("test7");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar --exclude file3 file1 >test.out 2>test.err", testprog));
	assertFileContents("file1", 5, "file1");
	assertFileNotExists("file2");
	assertFileNotExists("file3");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 8: Include a non-existent file */
	assertMakeDir("test8", 0755);
	assertChdir("test8");
	r = systemf("%s -xf ../archive.tar file3 >test.out 2>test.err", testprog);
	assert(r != 0);
	assertFileNotExists("file1");
	assertFileNotExists("file2");
	assertFileNotExists("file3");
	assertEmptyFile("test.out");
	assertNonEmptyFile("test.err");
	assertChdir("..");

	/* Test 9: Include a non-existent file plus an exclusion */
	assertMakeDir("test9", 0755);
	assertChdir("test9");
	r = systemf("%s -xf ../archive.tar --exclude file1 file3 >test.out 2>test.err", testprog);
	assert(r != 0);
	assertFileNotExists("file1");
	assertFileNotExists("file2");
	assertFileNotExists("file3");
	assertEmptyFile("test.out");
	assertNonEmptyFile("test.err");
	assertChdir("..");

#if !defined(_WIN32) || defined(__CYGWIN__)
	/*
	 * Tests 10 to 12: --exclude also catches a canonically equivalent
	 * spelling of the name, in both directions (CVE-2026-45110).  "caf"
	 * plus U+00E9 and "cafe" plus U+0301 are the same name to a
	 * filesystem that folds normalization, such as APFS.
	 */
	assertMakeFile("caf\xC3\xA9.txt", 0644, "nfc");
	assertMakeFile("cafe\xCC\x81_2.txt", 0644, "nfd");
	/* ustar stores the name bytes as given, whatever the locale is. */
	assertEqualInt(0, systemf("%s --format ustar -cf archive-nfc.tar "
	    "caf\xC3\xA9.txt", testprog));
	assertEqualInt(0, systemf("%s --format ustar -cf archive-nfd.tar "
	    "cafe\xCC\x81_2.txt", testprog));

	/* Test 10: an NFD entry excluded by an NFC pattern */
	assertMakeDir("test10", 0755);
	assertChdir("test10");
	assertEqualInt(0, systemf("%s -xf ../archive-nfd.tar "
	    "--exclude caf\xC3\xA9_2.txt >test.out 2>test.err", testprog));
	assertFileNotExists("cafe\xCC\x81_2.txt");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 11: an NFC entry excluded by an NFD pattern */
	assertMakeDir("test11", 0755);
	assertChdir("test11");
	assertEqualInt(0, systemf("%s -xf ../archive-nfc.tar "
	    "--exclude cafe\xCC\x81.txt >test.out 2>test.err", testprog));
	assertFileNotExists("caf\xC3\xA9.txt");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 12: an unrelated pattern must not exclude it */
	assertMakeDir("test12", 0755);
	assertChdir("test12");
	assertEqualInt(0, systemf("%s -xf ../archive-nfc.tar "
	    "--exclude other.txt >test.out 2>test.err", testprog));
	assertFileContents("nfc", 3, "caf\xC3\xA9.txt");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");
#endif
}
