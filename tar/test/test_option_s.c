/*-
 * Copyright (c) 2003-2008 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD: src/usr.bin/tar/test/test_option_T.c,v 1.3 2008/08/15 06:12:02 kientzle Exp $");

DEFINE_TEST(test_option_s)
{
	struct stat st;

	/* Create a sample file hierarchy. */
	assertMakeDir("in", 0755);
	assertMakeDir("in/d1", 0755);
	assertMakeFile("in/d1/foo", 0644, "foo");
	assertMakeFile("in/d1/bar", 0644, "bar");
	if (canSymlink()) {
		assertMakeFile("in/d1/realfile", 0644, "realfile");
		assertMakeSymlink("in/d1/symlink", "realfile");
	}
	assertMakeFile("in/d1/hardlink1", 0644, "hardlinkedfile");
	assertMakeHardlink("in/d1/hardlink2", "in/d1/hardlink1");

	/* Does tar support -s option ? */
	systemf("%s -cf - -s /foo/bar/ in/d1/foo > NUL 2> check.err",
	    testprog);
	assertEqualInt(0, stat("check.err", &st));
	if (st.st_size != 0) {
		skipping("%s does not support -s option on this platform",
			testprog);
		return;
	}

	/*
	 * Test 1: Filename substitution when creating archives.
	 */
	assertMakeDir("test1", 0755);
	systemf("%s -cf - -s /foo/bar/ in/d1/foo | %s -xf - -C test1",
	    testprog, testprog);
	assertFileContents("foo", 3, "test1/in/d1/bar");
	systemf("%s -cf - -s /d1/d2/ in/d1/foo | %s -xf - -C test1",
	    testprog, testprog);
	assertFileContents("foo", 3, "test1/in/d2/foo");

	/*
	 * Test 2: Basic substitution when extracting archive.
	 */
	assertMakeDir("test2", 0755);
	systemf("%s -cf - in/d1/foo | %s -xf - -s /foo/bar/ -C test2",
	    testprog, testprog);
	assertFileContents("foo", 3, "test2/in/d1/bar");

	/*
	 * Test 3: Files with empty names shouldn't be archived.
	 */
	systemf("%s -cf - -s ,in/d1/foo,, in/d1/foo | %s -tvf - > in.lst",
	    testprog, testprog);
	assertEmptyFile("in.lst");

	/*
	 * Test 4: Multiple substitutions when extracting archive.
	 */
	assertMakeDir("test4", 0755);
	systemf("%s -cf - in/d1/foo in/d1/bar | %s -xf - -s /foo/bar/ -s }bar}baz} -C test4",
	    testprog, testprog);
	assertFileContents("foo", 3, "test4/in/d1/bar");
	assertFileContents("bar", 3, "test4/in/d1/baz");

	/*
	 * Test 5: Name-switching substitutions when extracting archive.
	 */
	assertMakeDir("test5", 0755);
	systemf("%s -cf - in/d1/foo in/d1/bar | %s -xf - -s /foo/bar/ -s }bar}foo} -C test5",
	    testprog, testprog);
	assertFileContents("foo", 3, "test5/in/d1/bar");
	assertFileContents("bar", 3, "test5/in/d1/foo");

	/*
	 * Test 6: symlinks get renamed by default
	 */
	if (canSymlink()) {
		/* At extraction time. */
		assertMakeDir("test6a", 0755);
		systemf("%s -cf - in/d1 | %s -xf - -s /d1/d2/ -C test6a",
		    testprog, testprog);
		assertFileContents("realfile", 8, "test6a/in/d2/realfile");
		assertFileContents("realfile", 8, "test6a/in/d2/symlink");
		assertIsSymlink("test6a/in/d2/symlink", "realfile");
		/* At creation time. */
		assertMakeDir("test6b", 0755);
		systemf("%s -cf - -s /d1/d2/ in/d1 | %s -xf - -C test6b",
		    testprog, testprog);
		assertFileContents("realfile", 8, "test6b/in/d2/realfile");
		assertFileContents("realfile", 8, "test6b/in/d2/symlink");
		assertIsSymlink("test6b/in/d2/symlink", "realfile");
	}

	/*
	 * Test 7: selective renaming of symlink target
	 */
	if (canSymlink()) {
		/* At extraction. */
		assertMakeDir("test7a", 0755);
		systemf("%s -cf - in/d1 | %s -xf - -s /realfile/realfile-renamed/ -C test7a",
		    testprog, testprog);
		assertFileContents("realfile", 8, "test7a/in/d1/realfile-renamed");
		assertFileContents("realfile", 8, "test7a/in/d1/symlink");
		assertIsSymlink("test7a/in/d1/symlink", "realfile-renamed");
		/* At creation. */
		assertMakeDir("test7b", 0755);
		systemf("%s -cf - -s /realfile/realfile-renamed/ in/d1 | %s -xf - -C test7b",
		    testprog, testprog);
		assertFileContents("realfile", 8, "test7b/in/d1/realfile-renamed");
		assertFileContents("realfile", 8, "test7b/in/d1/symlink");
		assertIsSymlink("test7b/in/d1/symlink", "realfile-renamed");
	}

	/*
	 * Test 8: hardlinks get renamed by default
	 */
	/* At extraction time. */
	assertMakeDir("test8a", 0755);
	systemf("%s -cf - in/d1 | %s -xf - -s /d1/d2/ -C test8a",
	    testprog, testprog);
	assertIsHardlink("test8a/in/d2/hardlink1", "test8a/in/d2/hardlink2");
	/* At creation time. */
	assertMakeDir("test8b", 0755);
	systemf("%s -cf - -s /d1/d2/ in/d1 | %s -xf - -C test8b",
	    testprog, testprog);
	assertIsHardlink("test8b/in/d2/hardlink1", "test8b/in/d2/hardlink2");

	/*
	 * Test 9: selective renaming of hardlink target
	 */
	if (canSymlink()) {
		/* At extraction. (assuming hardlink2 is the hardlink entry) */
		assertMakeDir("test9a", 0755);
		systemf("%s -cf - in/d1 | %s -xf - -s /hardlink1/hardlink1-renamed/ -C test9a",
		    testprog, testprog);
		assertIsHardlink("test9a/in/d1/hardlink1-renamed", "test9a/in/d1/hardlink2");
		/* At extraction. (assuming hardlink1 is the hardlink entry) */
		assertMakeDir("test9b", 0755);
		systemf("%s -cf - in/d1 | %s -xf - -s /hardlink2/hardlink2-renamed/ -C test9b",
		    testprog, testprog);
		assertIsHardlink("test9b/in/d1/hardlink1", "test9b/in/d1/hardlink2-renamed");
		/* At creation. (assuming hardlink2 is the hardlink entry) */
		assertMakeDir("test9c", 0755);
		systemf("%s -cf - -s /hardlink1/hardlink1-renamed/ in/d1 | %s -xf - -C test9c",
		    testprog, testprog);
		assertIsHardlink("test9c/in/d1/hardlink1-renamed", "test9c/in/d1/hardlink2");
		/* At creation. (assuming hardlink1 is the hardlink entry) */
		assertMakeDir("test9d", 0755);
		systemf("%s -cf - -s /hardlink2/hardlink2-renamed/ in/d1 | %s -xf - -C test9d",
		    testprog, testprog);
		assertIsHardlink("test9d/in/d1/hardlink1", "test9d/in/d1/hardlink2-renamed");
	}

}
