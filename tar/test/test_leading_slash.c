/*-
 * Copyright (c) 2003-2014 Tim Kientzle
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

#ifdef _WIN32
# define BSDTAR_CMD "bsdtar.exe"
#elif defined(STATIC_BSDTAR)
# define BSDTAR_CMD "bsdtar"
#else
# define BSDTAR_CMD "lt-bsdtar"
#endif

DEFINE_TEST(test_leading_slash)
{
	const char *reffile = "test_leading_slash.tar";

	extract_reference_file(reffile);
	assertEqualInt(0, systemf("%s -xf %s >test.out 2>test.err", testprog, reffile));
	assertFileExists("foo/file");
	assertTextFileContents("foo\x0a", "foo/file");
	assertTextFileContents("foo\x0a", "foo/hardlink");
	assertIsHardlink("foo/file", "foo/hardlink");
	assertEmptyFile("test.out");
	assertTextFileContents(BSDTAR_CMD ": Removing leading '/' from member names\x0a", "test.err");
}

