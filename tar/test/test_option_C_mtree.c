/*-
 * Copyright (c) 2018 Arshan Khanifar
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

DEFINE_TEST(test_option_C_mtree)
{
	char *buff;
	char *p0;
	size_t s;
	int r;

	buff = NULL;
	p0 = NULL;

	char *content = "./foo type=file uname=root gname=root mode=0755\n";

	/* Create an archive using an mtree file. */
	assertMakeFile("/tmp/METALOG.mtree", 0777, content);
	assertMakeDir("bar", 0775);
	assertMakeFile("bar/foo", 0777, "abc");

	r = systemf("%s -cf output.tar -C bar @/tmp/METALOG.mtree >step1.out 2>step1.err", testprog);

	failure("Error invoking %s -cf output.tar -C bar @/tmp/METALOG.mtree", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("step1.out");
	assertEmptyFile("step1.err");

	/* Do validation of the constructed archive. */

	p0 = slurpfile(&s, "output.tar");
	if (!assert(p0 != NULL))
		goto done;
	if (!assert(s >= 2048))
		goto done;
	assertEqualMem(p0 + 0, "./foo", 5);
	assertEqualMem(p0 + 512, "abc", 3);
	assertEqualMem(p0 + 1024, "\0\0\0\0\0\0\0\0", 8);
	assertEqualMem(p0 + 1536, "\0\0\0\0\0\0\0\0", 8);

	free(p0);
done:
	free(buff);
	free(p0);
}


