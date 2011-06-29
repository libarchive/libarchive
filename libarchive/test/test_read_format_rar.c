/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2011 Andres Mejia
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

static void
test_basic(void)
{
  char buff[64];
  const char reffile[] = "test_read_format_rar.rar";
  const char test_txt[] = "test text document\r\n";
  int size = sizeof(test_txt)-1;
  struct archive_entry *ae;
  struct archive *a;

  extract_reference_file(reffile);
  assert((a = archive_read_new()) != NULL);
  assertA(0 == archive_read_support_filter_all(a));
  assertA(0 == archive_read_support_format_all(a));
  assertA(0 == archive_read_open_file(a, reffile, 10240));

  /* First header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("test.txt", archive_entry_pathname(ae));
  assertA(archive_entry_mtime(ae));
  assertA(archive_entry_ctime(ae));
  assertA(archive_entry_atime(ae));
  assertEqualInt(20, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(size == archive_read_data(a, buff, size));
  assertEqualMem(buff, test_txt, size);

  /* Second header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testlink", archive_entry_pathname(ae));
  assertA(archive_entry_mtime(ae));
  assertA(archive_entry_ctime(ae));
  assertA(archive_entry_atime(ae));
  assertEqualInt(8, archive_entry_size(ae));
  assertEqualInt(41471, archive_entry_mode(ae));
  assertEqualString("test.txt", archive_entry_symlink(ae));

  /* Third header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testdir/test.txt", archive_entry_pathname(ae));
  assertA(archive_entry_mtime(ae));
  assertA(archive_entry_ctime(ae));
  assertA(archive_entry_atime(ae));
  assertEqualInt(20, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(size == archive_read_data(a, buff, size));
  assertEqualMem(buff, test_txt, size);

  /* Fourth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testdir", archive_entry_pathname(ae));
  assertA(archive_entry_mtime(ae));
  assertA(archive_entry_ctime(ae));
  assertA(archive_entry_atime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Fifth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testemptydir", archive_entry_pathname(ae));
  assertA(archive_entry_mtime(ae));
  assertA(archive_entry_ctime(ae));
  assertA(archive_entry_atime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Test EOF */
  assertA(1 == archive_read_next_header(a, &ae));
  assertEqualInt(5, archive_file_count(a));
  assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
  assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_subblock(void)
{
  char buff[64];
  const char reffile[] = "test_read_format_rar_subblock.rar";
  const char test_txt[] = "test text document\r\n";
  int size = sizeof(test_txt)-1;
  struct archive_entry *ae;
  struct archive *a;

  extract_reference_file(reffile);
  assert((a = archive_read_new()) != NULL);
  assertA(0 == archive_read_support_filter_all(a));
  assertA(0 == archive_read_support_format_all(a));
  assertA(0 == archive_read_open_file(a, reffile, 10240));

  /* First header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("test.txt", archive_entry_pathname(ae));
  assertA(archive_entry_mtime(ae));
  assertA(archive_entry_ctime(ae));
  assertA(archive_entry_atime(ae));
  assertEqualInt(20, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(size == archive_read_data(a, buff, size));
  assertEqualMem(buff, test_txt, size);

  /* Test EOF */
  assertA(1 == archive_read_next_header(a, &ae));
  assertEqualInt(1, archive_file_count(a));
  assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
  assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_noeof(void)
{
  char buff[64];
  const char reffile[] = "test_read_format_rar_noeof.rar";
  const char test_txt[] = "test text document\r\n";
  int size = sizeof(test_txt)-1;
  struct archive_entry *ae;
  struct archive *a;

  extract_reference_file(reffile);
  assert((a = archive_read_new()) != NULL);
  assertA(0 == archive_read_support_filter_all(a));
  assertA(0 == archive_read_support_format_all(a));
  assertA(0 == archive_read_open_file(a, reffile, 10240));

  /* First header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("test.txt", archive_entry_pathname(ae));
  assertA(archive_entry_mtime(ae));
  assertA(archive_entry_ctime(ae));
  assertA(archive_entry_atime(ae));
  assertEqualInt(20, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(size == archive_read_data(a, buff, size));
  assertEqualMem(buff, test_txt, size);

  /* Test EOF */
  assertA(1 == archive_read_next_header(a, &ae));
  assertEqualInt(1, archive_file_count(a));
  assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
  assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_rar)
{
  test_basic();
  test_subblock();
  test_noeof();
}
