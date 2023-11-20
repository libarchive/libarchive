/*-
 * Copyright (c) 2010 Tim Kientzle
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

DEFINE_TEST(test_option_L_upper)
{

	if (!canSymlink()) {
		skipping("Can't test symlinks on this filesystem");
		return;
	}

	/*
	 * Create a sample archive.
	 */
	assertMakeDir("in", 0755);
	assertChdir("in");
	assertMakeDir("d1", 0755);
	assertMakeSymlink("ld1", "d1", 1);
	assertMakeFile("d1/file1", 0644, "d1/file1");
	assertMakeFile("d1/file2", 0644, "d1/file2");
	assertMakeSymlink("d1/link1", "file1", 0);
	assertMakeSymlink("d1/linkX", "fileX", 0);
	assertMakeSymlink("link2", "d1/file2", 0);
	assertMakeSymlink("linkY", "d1/fileY", 0);
	assertChdir("..");

	/* Test 1: Without -L */
	assertMakeDir("test1", 0755);
	assertEqualInt(0,
	    systemf("%s -cf test1/archive.tar -C in . >test1/c.out 2>test1/c.err", testprog));
	assertChdir("test1");
	assertEqualInt(0,
	    systemf("%s -xf archive.tar >c.out 2>c.err", testprog));
	assertIsSymlink("ld1", "d1", 1);
	assertIsSymlink("d1/link1", "file1", 0);
	assertIsSymlink("d1/linkX", "fileX", 0);
	assertIsSymlink("link2", "d1/file2", 0);
	assertIsSymlink("linkY", "d1/fileY", 0);
	assertChdir("..");

	/* Test 2: With -L, no symlink on command line. */
	assertMakeDir("test2", 0755);
	assertEqualInt(0,
	    systemf("%s -cf test2/archive.tar -L -C in . >test2/c.out 2>test2/c.err", testprog));
	assertChdir("test2");
	assertEqualInt(0,
	    systemf("%s -xf archive.tar >c.out 2>c.err", testprog));
	assertIsDir("ld1", umasked(0755));
	assertIsReg("d1/link1", umasked(0644));
	assertIsSymlink("d1/linkX", "fileX", 0);
	assertIsReg("link2", umasked(0644));
	assertIsSymlink("linkY", "d1/fileY", 0);
	assertChdir("..");

	/* Test 3: With -L, some symlinks on command line. */
	assertMakeDir("test3", 0755);
	assertEqualInt(0,
	    systemf("%s -cf test3/archive.tar -L -C in ld1 d1 link2 linkY >test2/c.out 2>test2/c.err", testprog));
	assertChdir("test3");
	assertEqualInt(0,
	    systemf("%s -xf archive.tar >c.out 2>c.err", testprog));
	assertIsDir("ld1", umasked(0755));
	assertIsReg("d1/link1", umasked(0644));
	assertIsSymlink("d1/linkX", "fileX", 0);
	assertIsReg("link2", umasked(0644));
	assertIsSymlink("linkY", "d1/fileY", 0);
	assertChdir("..");
}
