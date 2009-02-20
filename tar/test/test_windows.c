/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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
#ifdef _WIN32

static void
mkfile(const char *name)
{
	int fd;

	fd = open(name, O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(5, write(fd, "01234", 5));
	close(fd);
}

static const char list1[] =
    "aaa/\r\naaa/file1\r\naaa/xxa/\r\naaa/xxb/\r\naaa/zzc/\r\n"
    "aaa/zzc/file1\r\naaa/xxb/file1\r\naaa/xxa/file1\r\naab/\r\n"
	"aac/\r\nabb/\r\nabc/\r\nabd/\r\n";
static const char list2[] =
    "bbb/\r\nbbb/file1\r\nbbb/xxa/\r\nbbb/xxb/\r\nbbb/zzc/\r\n"
    "bbb/zzc/file1\r\nbbb/xxb/file1\r\nbbb/xxa/file1\r\nbbc/\r\n"
    "bbd/\r\nbcc/\r\nbcd/\r\nbce/\r\n";
static const char list3[] =
    "aac/\r\nabc/\r\nbbc/\r\nbcc/\r\nccc/\r\n";
static const char list4[] =
    "fff/abca\r\nfff/acca\r\n";
static const char list5[] =
    "aaa\\\\file1\r\naaa\\\\xxa/\r\naaa\\\\xxa/file1\r\naaa\\\\xxb/\r\n"
    "aaa\\\\xxb/file1\r\naaa\\\\zzc/\r\naaa\\\\zzc/file1\r\n";
static const char list6[] =
    "fff\\\\abca\r\nfff\\\\acca\r\naaa\\\\xxa/\r\naaa\\\\xxa/file1\r\n"
    "aaa\\\\xxb/\r\naaa\\\\xxb/file1\r\n";

DEFINE_TEST(test_windows)
{

	/*
	 * Create directories and files.
	 */
	assertEqualInt(0, mkdir("tmp", 0775));
	assertEqualInt(0, chdir("tmp"));

	assertEqualInt(0, mkdir("aaa", 0775));
	assertEqualInt(0, mkdir("aaa/xxa", 0775));
	assertEqualInt(0, mkdir("aaa/xxb", 0775));
	assertEqualInt(0, mkdir("aaa/zzc", 0775));
	mkfile("aaa/file1");
	mkfile("aaa/xxa/file1");
	mkfile("aaa/xxb/file1");
	mkfile("aaa/zzc/file1");
	assertEqualInt(0, mkdir("aab", 0775));
	assertEqualInt(0, mkdir("aac", 0775));
	assertEqualInt(0, mkdir("abb", 0775));
	assertEqualInt(0, mkdir("abc", 0775));
	assertEqualInt(0, mkdir("abd", 0775));
	assertEqualInt(0, mkdir("bbb", 0775));
	assertEqualInt(0, mkdir("bbb/xxa", 0775));
	assertEqualInt(0, mkdir("bbb/xxb", 0775));
	assertEqualInt(0, mkdir("bbb/zzc", 0775));
	mkfile("bbb/file1");
	mkfile("bbb/xxa/file1");
	mkfile("bbb/xxb/file1");
	mkfile("bbb/zzc/file1");
	assertEqualInt(0, mkdir("bbc", 0775));
	assertEqualInt(0, mkdir("bbd", 0775));
	assertEqualInt(0, mkdir("bcc", 0775));
	assertEqualInt(0, mkdir("bcd", 0775));
	assertEqualInt(0, mkdir("bce", 0775));
	assertEqualInt(0, mkdir("ccc", 0775));
	assertEqualInt(0, mkdir("fff", 0775));
	mkfile("fff/aaaa");
	mkfile("fff/abba");
	mkfile("fff/abca");
	mkfile("fff/acba");
	mkfile("fff/acca");

	/*
	 * command line pattern matching tests
	 */
	assertEqualInt(0,
	    systemf("%s -cf ../archive.tar a*", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive.tar > ../list", testprog));
	assertFileContents(list1, sizeof(list1)-1, "../list");

	assertEqualInt(0,
	    systemf("%s -cf ../archive.tar b*", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive.tar > ../list", testprog));
	assertFileContents(list2, sizeof(list2)-1, "../list");

	assertEqualInt(0,
	    systemf("%s -cf ../archive.tar ??c", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive.tar > ../list", testprog));
	assertFileContents(list3, sizeof(list3)-1, "../list");

	assertEqualInt(0,
	    systemf("%s -cf ../archive.tar *c", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive.tar > ../list", testprog));
	assertFileContents(list3, sizeof(list3)-1, "../list");

	assertEqualInt(0,
	    systemf("%s -cf ../archive.tar fff/a?ca", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive.tar > ../list", testprog));
	assertFileContents(list4, sizeof(list4)-1, "../list");

	assertEqualInt(0,
	    systemf("%s -cf ../archive.tar aaa\\*", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive.tar > ../list", testprog));
	assertFileContents(list5, sizeof(list5)-1, "../list");

	assertEqualInt(0,
	    systemf("%s -cf ../archive.tar fff\\a?ca aaa\\xx*", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive.tar > ../list", testprog));
	assertFileContents(list6, sizeof(list6)-1, "../list");

}
#endif /* _WIN32 */
