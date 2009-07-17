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
__FBSDID("$FreeBSD: src/usr.bin/tar/test/test_basic.c,v 1.2 2008/05/26 17:10:10 kientzle Exp $");


DEFINE_TEST(test_option_r)
{
	char buff[15];
	FILE *f;
	int r;

	/* Create a file */
	f = fopen("f1", "w");
	if (!assert(f != NULL))
		return;
	assertEqualInt(3, fwrite("abc", 1, 3, f));
	fclose(f);

	/* Archive that one file. */
	r = systemf("%s cf archive.tar f1 >step1.out 2>step1.err", testprog);
	failure("Error invoking %s cf archive.tar f1", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout or stderr. */
	assertEmptyFile("step1.out");
	assertEmptyFile("step1.err");

	/* Edit that file */
	f = fopen("f1", "w");
	if (!assert(f != NULL))
		return;
	assertEqualInt(3, fwrite("123", 1, 3, f));
	fclose(f);

	/* Update the archive. */
	r = systemf("%s rf archive.tar f1 >step2.out 2>step2.err", testprog);
	failure("Error invoking %s rf archive.tar f1", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stderr. */
	assertEmptyFile("step2.out");
	assertEmptyFile("step2.err");

	/* Unpack both items */
	assertEqualInt(0, mkdir("step3", 0775));
	assertEqualInt(0, chdir("step3"));
	r = systemf("%s xf ../archive.tar", testprog);
	failure("Error invoking %s xf archive.tar", testprog);
	assertEqualInt(r, 0);

	/* Verify that the second one overwrote the first. */
	f = fopen("f1", "r");
	if (assert(f != NULL)) {
		assertEqualInt(3, fread(buff, 1, 3, f));
		assertEqualMem(buff, "123", 3);
		fclose(f);
	}

	/* Unpack just the first item.  (This also verifies that the
	* 'r' update actually appended and didn't just overwrite.) */
	assertEqualInt(0, chdir(".."));
	assertEqualInt(0, mkdir("step4", 0775));
	assertEqualInt(0, chdir("step4"));
	r = systemf("%s xqf ../archive.tar f1", testprog);
	failure("Error invoking %s xqf archive.tar f1", testprog);
	assertEqualInt(r, 0);

	/* Verify the first file got extracted. */
	f = fopen("f1", "r");
	if (assert(f != NULL)) {
		assertEqualInt(3, fread(buff, 1, 3, f));
		assertEqualMem(buff, "abc", 3);
		fclose(f);
	}

}
