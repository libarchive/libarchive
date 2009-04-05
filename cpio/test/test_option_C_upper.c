/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
__FBSDID("$FreeBSD$");


DEFINE_TEST(test_option_upper_C)
{
	struct stat st;
	int r, fd;

	/*
	 * Create a file on disk.
	 */
	fd = open("file", O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	close(fd);

	/* Create an archive without -C; this should be 512 bytes. */
	r = systemf("echo file | %s -o > small.cpio 2>small.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "small.err");
	assertEqualInt(0, stat("small.cpio", &st));
	assertEqualInt(512, st.st_size);

	/* Create an archive with -C 513; this should be 513 bytes. */
	r = systemf("echo file | %s -o -C 513 > 513.cpio 2>513.err",
		    testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "513.err");
	assertEqualInt(0, stat("513.cpio", &st));
	assertEqualInt(513, st.st_size);

	/* Create an archive with -C 12345; this should be 12345 bytes. */
	r = systemf("echo file | %s -o -C12345 > 12345.cpio 2>12345.err",
		    testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "12345.err");
	assertEqualInt(0, stat("12345.cpio", &st));
	assertEqualInt(12345, st.st_size);

	/* Create an archive with invalid -C request */
	assert(0 != systemf("echo file | %s -o -C > bad.cpio 2>bad.err",
			    testprog));
	assertEmptyFile("bad.cpio");
}
