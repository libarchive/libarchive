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
__FBSDID("$FreeBSD$");

DEFINE_TEST(test_option_newer_than)
{
  struct stat st;

  /*
   * Basic test of --newer-than.
   * First, create three files with different mtimes.
   * Create test1.tar with --newer-than, test2.tar without.
   */
  assertMakeDir("test1in", 0755);
  assertChdir("test1in");
  assertMakeFile("old.txt", 0644, "old.txt");
  assertEqualInt(0, stat("old.txt", &st));
  sleepUntilAfter(st.st_mtime);
  assertMakeFile("middle.txt", 0644, "middle.txt");
  assertEqualInt(0, stat("middle.txt", &st));
  sleepUntilAfter(st.st_mtime);
  assertMakeFile("new.txt", 0644, "new");

  /* Test --newer-than on create */
  assertEqualInt(0, systemf("%s -cf ../test1.tar --newer-than middle.txt *.txt", testprog));
  assertEqualInt(0, systemf("%s -cf ../test2.tar *.txt", testprog));
  assertChdir("..");

  /* Extract test1.tar to a clean dir and verify what got archived. */
  assertMakeDir("test1out", 0755);
  assertChdir("test1out");
  assertEqualInt(0, systemf("%s xf ../test1.tar", testprog));
  assertFileExists("new.txt");
  assertFileNotExists("middle.txt");
  assertFileNotExists("old.txt");
  assertChdir("..");

  /* Extract test2.tar to a clean dir with --newer-than and verify. */
  assertMakeDir("test2out", 0755);
  assertChdir("test2out");
  assertEqualInt(0, systemf("%s xf ../test2.tar --newer-than ../test1in/middle.txt", testprog));
  assertFileExists("new.txt");
  assertFileNotExists("middle.txt");
  assertFileNotExists("old.txt");
  assertChdir("..");

  /* TODO: old dir containing new file, should descend into dir to visit file even though we don't archive it. */
  // rm -rf t && mkdir -p t/a/b && cd t && sleep 1 && touch TIMESTAMP && sleep 1 && touch a/b/test && ../bin/bsdtar cvf /dev/null --newer-than TIMESTAMP a && cd ..

}
