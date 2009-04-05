/*-
 * Copyright (c) 2003-2009 Tim Kientzle
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

/*
 * Test the command-line parsing.
 */

DEFINE_TEST(test_cmdline)
{
	failure("-Q is an invalid option on every cpio program I know of");
	assert(0 != systemf("%s -i -Q >1.stdout 2>1.stderr", testprog));
	assertEmptyFile("1.stdout");

	failure("-f requires an argument");
	assert(0 != systemf("%s -if >2.stdout 2>2.stderr", testprog));
	assertEmptyFile("2.stdout");

	failure("-f requires an argument");
	assert(0 != systemf("%s -i -f >3.stdout 2>3.stderr", testprog));
	assertEmptyFile("3.stdout");

	failure("--format requires an argument");
	assert(0 != systemf("%s -i --format >4.stdout 2>4.stderr", testprog));
	assertEmptyFile("4.stdout");

	failure("--badopt is an invalid option");
	assert(0 != systemf("%s -i --badop >5.stdout 2>5.stderr", testprog));
	assertEmptyFile("5.stdout");

	failure("--badopt is an invalid option");
	assert(0 != systemf("%s -i --badopt >6.stdout 2>6.stderr", testprog));
	assertEmptyFile("6.stdout");

	failure("--n is ambiguous");
	assert(0 != systemf("%s -i --n >7.stdout 2>7.stderr", testprog));
	assertEmptyFile("7.stdout");

	failure("--create forbids an argument");
	assert(0 != systemf("%s --create=arg >8.stdout 2>8.stderr", testprog));
	assertEmptyFile("8.stdout");
}
