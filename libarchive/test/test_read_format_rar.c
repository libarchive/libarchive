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

#include <locale.h>

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
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(20, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(size == archive_read_data(a, buff, size));
  assertEqualMem(buff, test_txt, size);

  /* Second header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testlink", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(8, archive_entry_size(ae));
  assertEqualInt(41471, archive_entry_mode(ae));
  assertEqualString("test.txt", archive_entry_symlink(ae));

  /* Third header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testdir/test.txt", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(20, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(size == archive_read_data(a, buff, size));
  assertEqualMem(buff, test_txt, size);

  /* Fourth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testdir", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Fifth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testemptydir", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
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
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
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
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
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
test_unicode_UTF8(void)
{
  char buff[5];
  const char reffile[] = "test_read_format_rar_unicode.rar";
  const char test_txt[] = "kanji";
  struct archive_entry *ae;
  struct archive *a;

  if (NULL == setlocale(LC_ALL, "en_US.UTF-8")) {
	skipping("en_US.UTF-8 locale not available on this system.");
	return;
  }

  extract_reference_file(reffile);
  assert((a = archive_read_new()) != NULL);
  assertA(0 == archive_read_support_filter_all(a));
  assertA(0 == archive_read_support_format_all(a));
  assertA(0 == archive_read_open_file(a, reffile, 10240));

  /* First header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualUTF8String("\xE8\xA1\xA8\xE3\x81\xA0\xE3\x82\x88/"
      "\xE6\x96\xB0\xE3\x81\x97\xE3\x81\x84\xE3\x83\x95\xE3\x82"
      "\xA9\xE3\x83\xAB\xE3\x83\x80/\xE6\x96\xB0\xE8\xA6\x8F"
      "\xE3\x83\x86\xE3\x82\xAD\xE3\x82\xB9\xE3\x83\x88 "
      "\xE3\x83\x89\xE3\x82\xAD\xE3\x83\xA5\xE3\x83\xA1\xE3\x83"
      "\xB3\xE3\x83\x88.txt", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));

  /* Second header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualUTF8String("\xE8\xA1\xA8\xE3\x81\xA0\xE3\x82\x88/"
      "\xE6\xBC\xA2\xE5\xAD\x97\xE9\x95\xB7\xE3\x81\x84\xE3\x83\x95"
      "\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB\xE5\x90\x8Dlong-filename-in-"
      "\xE6\xBC\xA2\xE5\xAD\x97.txt", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertEqualInt(5, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(5 == archive_read_data(a, buff, 5));
  assertEqualMem(buff, test_txt, 5);

  /* Third header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualUTF8String("\xE8\xA1\xA8\xE3\x81\xA0\xE3\x82\x88/"
      "\xE6\x96\xB0\xE3\x81\x97\xE3\x81\x84\xE3\x83\x95\xE3\x82"
      "\xA9\xE3\x83\xAB\xE3\x83\x80", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Fourth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualUTF8String("\xE8\xA1\xA8\xE3\x81\xA0\xE3\x82\x88",
      archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Test EOF */
  assertA(1 == archive_read_next_header(a, &ae));
  assertEqualInt(4, archive_file_count(a));
  assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
  assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_unicode_CP932(void)
{
  char buff[5];
  const char reffile[] = "test_read_format_rar_unicode.rar";
  const char test_txt[] = "kanji";
  struct archive_entry *ae;
  struct archive *a;

  if (NULL == setlocale(LC_ALL, "Japanese_Japan") &&
    NULL == setlocale(LC_ALL, "ja_JP.SJIS")) {
	skipping("CP932 locale not available on this system.");
	return;
  }

  /*
   * Test that current platform supports a string conversion from UTF-8
   * to CP932.
   * Note: use the zip reader since the rar reader does not support
   * a hdrcharset option.
   */
  assert((a = archive_read_new()) != NULL);
  assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
  if (ARCHIVE_OK != archive_read_set_options(a, "hdrcharset=UTF-8")) {
	skipping("This system cannot convert character-set"
	    " from Unicode to CP932.");
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	return;
  }
  assertEqualInt(ARCHIVE_OK, archive_read_free(a));

  extract_reference_file(reffile);
  assert((a = archive_read_new()) != NULL);
  assertA(0 == archive_read_support_filter_all(a));
  assertA(0 == archive_read_support_format_all(a));
  assertA(0 == archive_read_open_file(a, reffile, 10240));

  /* First header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("\x95\x5c\x82\xbe\x82\xe6/\x90\x56\x82\xb5\x82\xa2"
      "\x83\x74\x83\x48\x83\x8b\x83\x5f/\x90\x56\x8b\x4b\x83\x65\x83\x4c"
      "\x83\x58\x83\x67 \x83\x68\x83\x4c\x83\x85\x83\x81\x83\x93\x83\x67.txt",
      archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));

  /* Second header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("\x95\x5c\x82\xbe\x82\xe6/\x8a\xbf\x8e\x9a"
      "\x92\xb7\x82\xa2\x83\x74\x83\x40\x83\x43\x83\x8b\x96\xbc\x6c"
      "\x6f\x6e\x67\x2d\x66\x69\x6c\x65\x6e\x61\x6d\x65\x2d\x69\x6e"
      "\x2d\x8a\xbf\x8e\x9a.txt", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertEqualInt(5, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(5 == archive_read_data(a, buff, 5));
  assertEqualMem(buff, test_txt, 5);

  /* Third header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("\x95\x5c\x82\xbe\x82\xe6/"
      "\x90\x56\x82\xb5\x82\xa2\x83\x74\x83\x48\x83\x8b\x83\x5f",
      archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Fourth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("\x95\x5c\x82\xbe\x82\xe6", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Test EOF */
  assertA(1 == archive_read_next_header(a, &ae));
  assertEqualInt(4, archive_file_count(a));
  assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
  assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_compress_normal(void)
{
  const char reffile[] = "test_read_format_rar_compress_normal.rar";
  const int file1_size = 20111;
  char file1_buff[file1_size];
  const char file1_test_txt[] = "<P STYLE=\"margin-bottom: 0in\"><BR>\n"
                                "</P>\n"
                                "</BODY>\n"
                                "</HTML>";
  const int file2_size = 20;
  char file2_buff[file2_size];
  const char file2_test_txt[] = "test text document\r\n";
  struct archive_entry *ae;
  struct archive *a;

  extract_reference_file(reffile);
  assert((a = archive_read_new()) != NULL);
  assertA(0 == archive_read_support_filter_all(a));
  assertA(0 == archive_read_support_format_all(a));
  assertA(0 == archive_read_open_file(a, reffile, 10240));

  /* First header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("LibarchiveAddingTest.html", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(file1_size, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(file1_size == archive_read_data(a, file1_buff, file1_size));
  assertEqualMem(&file1_buff[file1_size - sizeof(file1_test_txt) + 1],
                 file1_test_txt, sizeof(file1_test_txt) - 1);

    /* Second header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testlink", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(25, archive_entry_size(ae));
  assertEqualInt(41471, archive_entry_mode(ae));
  assertEqualString("LibarchiveAddingTest.html", archive_entry_symlink(ae));

  /* Third header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testdir/test.txt", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(file2_size, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(file2_size == archive_read_data(a, file2_buff, file2_size));
  assertEqualMem(&file2_buff[file2_size + 1 - sizeof(file2_test_txt)],
                 file2_test_txt, sizeof(file2_test_txt) - 1);

  /* Fourth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testdir/LibarchiveAddingTest.html",
                    archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(file1_size, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(file1_size == archive_read_data(a, file1_buff, file1_size));
  assertEqualMem(&file1_buff[file1_size - sizeof(file1_test_txt) + 1],
                 file1_test_txt, sizeof(file1_test_txt) - 1);

  /* Fifth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testdir", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Sixth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testemptydir", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Test EOF */
  assertA(1 == archive_read_next_header(a, &ae));
  assertEqualInt(6, archive_file_count(a));
  assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
  assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* This test is for sufficiently large files that would have been compressed
 * using multiple lzss blocks.
 */
static void
test_multi_lzss_blocks(void)
{
  const char reffile[] = "test_read_format_rar_multi_lzss_blocks.rar";
  const char test_txt[] = "-bottom: 0in\"><BR>\n</P>\n</BODY>\n</HTML>";
  int size = 20131111, offset = 0;
  char buff[64];
  struct archive_entry *ae;
  struct archive *a;

  extract_reference_file(reffile);
  assert((a = archive_read_new()) != NULL);
  assertA(0 == archive_read_support_filter_all(a));
  assertA(0 == archive_read_support_format_all(a));
  assertA(0 == archive_read_open_file(a, reffile, 10240));

  /* First header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("multi_lzss_blocks_test.txt", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(size, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  while (offset + (int)sizeof(buff) < size)
  {
    assertA(sizeof(buff) == archive_read_data(a, buff, sizeof(buff)));
    offset += sizeof(buff);
  }
  assertA(size - offset == archive_read_data(a, buff, size - offset));
  assertEqualMem(buff, test_txt, size - offset);

  /* Test EOF */
  assertA(1 == archive_read_next_header(a, &ae));
  assertEqualInt(1, archive_file_count(a));
  assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
  assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_compress_best(void)
{
  const char reffile[] = "test_read_format_rar_compress_best.rar";
  const int file1_size = 20111;
  char file1_buff[file1_size];
  const char file1_test_txt[] = "<P STYLE=\"margin-bottom: 0in\"><BR>\n"
                                "</P>\n"
                                "</BODY>\n"
                                "</HTML>";
  const int file2_size = 20;
  char file2_buff[file2_size];
  const char file2_test_txt[] = "test text document\r\n";
  struct archive_entry *ae;
  struct archive *a;

  extract_reference_file(reffile);
  assert((a = archive_read_new()) != NULL);
  assertA(0 == archive_read_support_filter_all(a));
  assertA(0 == archive_read_support_format_all(a));
  assertA(0 == archive_read_open_file(a, reffile, 10240));

  /* First header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("LibarchiveAddingTest.html", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(file1_size, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(file1_size == archive_read_data(a, file1_buff, file1_size));
  assertEqualMem(&file1_buff[file1_size - sizeof(file1_test_txt) + 1],
                 file1_test_txt, sizeof(file1_test_txt) - 1);

    /* Second header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testlink", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(25, archive_entry_size(ae));
  assertEqualInt(41471, archive_entry_mode(ae));
  assertEqualString("LibarchiveAddingTest.html", archive_entry_symlink(ae));

  /* Third header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testdir/test.txt", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(file2_size, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(file2_size == archive_read_data(a, file2_buff, file2_size));
  assertEqualMem(&file2_buff[file2_size + 1 - sizeof(file2_test_txt)],
                 file2_test_txt, sizeof(file2_test_txt) - 1);

  /* Fourth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testdir/LibarchiveAddingTest.html",
                    archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(file1_size, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  assertA(file1_size == archive_read_data(a, file1_buff, file1_size));
  assertEqualMem(&file1_buff[file1_size - sizeof(file1_test_txt) + 1],
                 file1_test_txt, sizeof(file1_test_txt) - 1);

  /* Fifth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testdir", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Sixth header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("testemptydir", archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(0, archive_entry_size(ae));
  assertEqualInt(16877, archive_entry_mode(ae));

  /* Test EOF */
  assertA(1 == archive_read_next_header(a, &ae));
  assertEqualInt(6, archive_file_count(a));
  assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
  assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* This is a test for RAR files compressed using a technique where compression
 * switches back and forth to and from ppmd and lzss decoding.
 */
static void
test_ppmd_lzss_conversion(void)
{
  const char reffile[] = "test_read_format_rar_ppmd_lzss_conversion.rar";
  const char test_txt[] = "-bottom: 0in\"><BR>\n</P>\n</BODY>\n</HTML>";
  int size = 20131111, offset = 0;
  char buff[64];
  struct archive_entry *ae;
  struct archive *a;

  extract_reference_file(reffile);
  assert((a = archive_read_new()) != NULL);
  assertA(0 == archive_read_support_filter_all(a));
  assertA(0 == archive_read_support_format_all(a));
  assertA(0 == archive_read_open_file(a, reffile, 10240));

  /* First header. */
  assertA(0 == archive_read_next_header(a, &ae));
  assertEqualString("ppmd_lzss_conversion_test.txt",
                    archive_entry_pathname(ae));
  assertA((int)archive_entry_mtime(ae));
  assertA((int)archive_entry_ctime(ae));
  assertA((int)archive_entry_atime(ae));
  assertEqualInt(size, archive_entry_size(ae));
  assertEqualInt(33188, archive_entry_mode(ae));
  while (offset + (int)sizeof(buff) < size)
  {
    assertA(sizeof(buff) == archive_read_data(a, buff, sizeof(buff)));
    offset += sizeof(buff);
  }
  assertA(size - offset == archive_read_data(a, buff, size - offset));
  assertEqualMem(buff, test_txt, size - offset);

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
  test_unicode_UTF8();
  test_unicode_CP932();
  test_compress_normal();
  test_multi_lzss_blocks();
  test_compress_best();
  test_ppmd_lzss_conversion();
}
