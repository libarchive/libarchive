/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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

DEFINE_TEST(test_option_uid_uname)
{
	char *reference, *data;
	size_t s;

	assertUmask(0);
	assertMakeFile("file", 0644, "1234567890");

	/* Create archive with no special options. */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive1 --format=ustar file >stdout1.txt 2>stderr1.txt",
		testprog));
	assertEmptyFile("stdout1.txt");
	assertEmptyFile("stderr1.txt");
	reference = slurpfile(&s, "archive1");

	/* Again with both --uid and --uname */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive2 --uid=65123 --uname=foofoofoo --format=ustar file >stdout2.txt 2>stderr2.txt",
		testprog));
	assertEmptyFile("stdout2.txt");
	assertEmptyFile("stderr2.txt");
	data = slurpfile(&s, "archive2");
	/* Should force uid and uname fields in ustar header. */
	assertEqualMem(data + 108, "177143 \0", 8);
	assertEqualMem(data + 265, "foofoofoo\0", 10);
	free(data);

	/* Again with just --uid */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive3 --uid=65123 --format=ustar file >stdout3.txt 2>stderr3.txt",
		testprog));
	assertEmptyFile("stdout3.txt");
	assertEmptyFile("stderr3.txt");
	data = slurpfile(&s, "archive3");
	assertEqualMem(data + 108, "177143 \0", 8);
	/* Uname field in ustar header should be empty. */
	assertEqualMem(data + 265, "\0", 1);
	free(data);

	/* Again with just --uname */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive4 --uname=foofoofoo --format=ustar file >stdout4.txt 2>stderr4.txt",
		testprog));
	assertEmptyFile("stdout4.txt");
	assertEmptyFile("stderr4.txt");
	data = slurpfile(&s, "archive4");
	/* Uid should be unchanged from original reference. */
	assertEqualMem(data + 108, reference + 108, 8);
	assertEqualMem(data + 265, "foofoofoo\0", 10);
	free(data);

	free(reference);
}
