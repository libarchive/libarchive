/*-
 * Copyright (c) 2011-2012 Michihiro NAKAJIMA
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

#define __LIBARCHIVE_TEST
#include "archive_write_private.h"

static void
test_basic(const char *compression_type)
{
	char filedata[64];
	struct archive_entry *ae;
	struct archive *a;
	size_t used;
	size_t buffsize = 1000;
	char *buff;

	buff = malloc(buffsize);

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_7zip(a));
	if (compression_type != NULL &&
	    ARCHIVE_OK != archive_write_set_format_option(a, "7zip",
	    "compression", compression_type)) {
		skipping("%s writing not fully supported on this platform",
		   compression_type);
		assertEqualInt(ARCHIVE_OK, archive_write_free(a));
		free(buff);
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	/*
	 * Write an empty file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 10);
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(10, archive_entry_mtime_nsec(ae));
	archive_entry_copy_pathname(ae, "empty");
	assertEqualString("empty", archive_entry_pathname(ae));
	archive_entry_set_mode(ae, AE_IFREG | 0755);
	assertEqualInt((AE_IFREG | 0755), archive_entry_mode(ae));

	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/*
	 * Write another empty file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 10);
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(10, archive_entry_mtime_nsec(ae));
	archive_entry_copy_pathname(ae, "empty2");
	assertEqualString("empty2", archive_entry_pathname(ae));
	archive_entry_set_mode(ae, AE_IFREG | 0444);
	assertEqualInt((AE_IFREG | 0444), archive_entry_mode(ae));

	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/*
	 * Write a file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 100);
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(100, archive_entry_mtime_nsec(ae));
	archive_entry_copy_pathname(ae, "file");
	assertEqualString("file", archive_entry_pathname(ae));
	archive_entry_set_mode(ae, AE_IFREG | 0755);
	assertEqualInt((AE_IFREG | 0755), archive_entry_mode(ae));
	archive_entry_set_size(ae, 8);

	assertEqualInt(0, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(8, archive_write_data(a, "12345678", 9));
	assertEqualInt(0, archive_write_data(a, "1", 1));

	/*
	 * Write another file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 10);
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(10, archive_entry_mtime_nsec(ae));
	archive_entry_copy_pathname(ae, "file2");
	assertEqualString("file2", archive_entry_pathname(ae));
	archive_entry_set_mode(ae, AE_IFREG | 0755);
	assertEqualInt((AE_IFREG | 0755), archive_entry_mode(ae));
	archive_entry_set_size(ae, 4);

	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(4, archive_write_data(a, "1234", 5));

	/*
	 * Write a symbolic file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 10);
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(10, archive_entry_mtime_nsec(ae));
	archive_entry_copy_pathname(ae, "symbolic");
	archive_entry_copy_symlink(ae, "file1");
	assertEqualString("symbolic", archive_entry_pathname(ae));
	archive_entry_set_mode(ae, AE_IFLNK | 0755);
	assertEqualInt((AE_IFLNK | 0755), archive_entry_mode(ae));

	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/*
	 * Write a directory to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 11, 100);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_set_size(ae, 512);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	failure("size should be zero so that applications know not to write");
	assertEqualInt(0, archive_entry_size(ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 0, archive_write_data(a, "12345678", 9));

	/*
	 * Write a sub directory to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 11, 200);
	archive_entry_copy_pathname(ae, "dir/subdir");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_set_size(ae, 512);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	failure("size should be zero so that applications know not to write");
	assertEqualInt(0, archive_entry_size(ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 0, archive_write_data(a, "12345678", 9));

	/*
	 * Write a sub sub-directory to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 11, 300);
	archive_entry_copy_pathname(ae, "dir/subdir/subdir");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_set_size(ae, 512);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	failure("size should be zero so that applications know not to write");
	assertEqualInt(0, archive_entry_size(ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 0, archive_write_data(a, "12345678", 9));

	/* Close out the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Verify the initial header. */
	assertEqualMem(buff, "\x37\x7a\xbc\xaf\x27\x1c\x00\x03", 8);

	/*
	 * Now, read the data back.
	 */
	/* With the test memory reader -- seeking mode. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, buff, used, 7));

	/*
	 * Read and verify first file.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(100, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(8, archive_entry_size(ae));
	assertEqualIntA(a, 8,
	    archive_read_data(a, filedata, sizeof(filedata)));
	assertEqualMem(filedata, "12345678", 8);


	/*
	 * Read the second file back.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(4, archive_entry_size(ae));
	assertEqualIntA(a, 4,
	    archive_read_data(a, filedata, sizeof(filedata)));
	assertEqualMem(filedata, "1234", 4);

	/*
	 * Read and verify a symbolic file.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("symbolic", archive_entry_pathname(ae));
	assertEqualString("file1", archive_entry_symlink(ae));
	assertEqualInt(AE_IFLNK | 0755, archive_entry_mode(ae));
	assertEqualInt(0, archive_entry_size(ae));

	/*
	 * Read and verify an empty file.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("empty", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(0, archive_entry_size(ae));

	/*
	 * Read and verify an empty file.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("empty2", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0444, archive_entry_mode(ae));
	assertEqualInt(0, archive_entry_size(ae));

	/*
	 * Read the sub sub-dir entry back.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(11, archive_entry_mtime(ae));
	assertEqualInt(300, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("dir/subdir/subdir/", archive_entry_pathname(ae));
	assertEqualInt(AE_IFDIR | 0755, archive_entry_mode(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_read_data(a, filedata, 10));

	/*
	 * Read the sub dir entry back.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(11, archive_entry_mtime(ae));
	assertEqualInt(200, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("dir/subdir/", archive_entry_pathname(ae));
	assertEqualInt(AE_IFDIR | 0755, archive_entry_mode(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_read_data(a, filedata, 10));

	/*
	 * Read the dir entry back.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(11, archive_entry_mtime(ae));
	assertEqualInt(100, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("dir/", archive_entry_pathname(ae));
	assertEqualInt(AE_IFDIR | 0755, archive_entry_mode(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_read_data(a, filedata, 10));

	/* Verify the end of the archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
}

static void
test_basic2(const char *compression_type)
{
	char filedata[64];
	struct archive_entry *ae;
	struct archive *a;
	size_t used;
	size_t buffsize = 1000;
	char *buff;

	buff = malloc(buffsize);

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_7zip(a));
	if (compression_type != NULL &&
	    ARCHIVE_OK != archive_write_set_format_option(a, "7zip",
	    "compression", compression_type)) {
		skipping("%s writing not fully supported on this platform",
		   compression_type);
		assertEqualInt(ARCHIVE_OK, archive_write_free(a));
		free(buff);
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	/*
	 * Write a file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 100);
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(100, archive_entry_mtime_nsec(ae));
	archive_entry_copy_pathname(ae, "file");
	assertEqualString("file", archive_entry_pathname(ae));
	archive_entry_set_mode(ae, AE_IFREG | 0755);
	assertEqualInt((AE_IFREG | 0755), archive_entry_mode(ae));
	archive_entry_set_size(ae, 8);

	assertEqualInt(0, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(8, archive_write_data(a, "12345678", 9));
	assertEqualInt(0, archive_write_data(a, "1", 1));

	/*
	 * Write another file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 10);
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(10, archive_entry_mtime_nsec(ae));
	archive_entry_copy_pathname(ae, "file2");
	assertEqualString("file2", archive_entry_pathname(ae));
	archive_entry_set_mode(ae, AE_IFREG | 0755);
	assertEqualInt((AE_IFREG | 0755), archive_entry_mode(ae));
	archive_entry_set_size(ae, 4);

	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(4, archive_write_data(a, "1234", 5));

	/*
	 * Write a directory to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 11, 100);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_set_size(ae, 512);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	failure("size should be zero so that applications know not to write");
	assertEqualInt(0, archive_entry_size(ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 0, archive_write_data(a, "12345678", 9));

	/*
	 * Write a sub directory to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 11, 200);
	archive_entry_copy_pathname(ae, "dir/subdir");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_set_size(ae, 512);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	failure("size should be zero so that applications know not to write");
	assertEqualInt(0, archive_entry_size(ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 0, archive_write_data(a, "12345678", 9));

	/*
	 * Write a sub sub-directory to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 11, 300);
	archive_entry_copy_pathname(ae, "dir/subdir/subdir");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_set_size(ae, 512);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	failure("size should be zero so that applications know not to write");
	assertEqualInt(0, archive_entry_size(ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 0, archive_write_data(a, "12345678", 9));

	/* Close out the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Verify the initial header. */
	assertEqualMem(buff, "\x37\x7a\xbc\xaf\x27\x1c\x00\x03", 8);

	/*
	 * Now, read the data back.
	 */
	/* With the test memory reader -- seeking mode. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, buff, used, 7));

	/*
	 * Read and verify first file.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(100, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(8, archive_entry_size(ae));
	assertEqualIntA(a, 8,
	    archive_read_data(a, filedata, sizeof(filedata)));
	assertEqualMem(filedata, "12345678", 8);


	/*
	 * Read the second file back.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(4, archive_entry_size(ae));
	assertEqualIntA(a, 4,
	    archive_read_data(a, filedata, sizeof(filedata)));
	assertEqualMem(filedata, "1234", 4);

	/*
	 * Read the sub sub-dir entry back.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(11, archive_entry_mtime(ae));
	assertEqualInt(300, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("dir/subdir/subdir/", archive_entry_pathname(ae));
	assertEqualInt(AE_IFDIR | 0755, archive_entry_mode(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_read_data(a, filedata, 10));

	/*
	 * Read the sub dir entry back.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(11, archive_entry_mtime(ae));
	assertEqualInt(200, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("dir/subdir/", archive_entry_pathname(ae));
	assertEqualInt(AE_IFDIR | 0755, archive_entry_mode(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_read_data(a, filedata, 10));

	/*
	 * Read the dir entry back.
	 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(11, archive_entry_mtime(ae));
	assertEqualInt(100, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("dir/", archive_entry_pathname(ae));
	assertEqualInt(AE_IFDIR | 0755, archive_entry_mode(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_read_data(a, filedata, 10));

	/* Verify the end of the archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
}

DEFINE_TEST(test_write_format_7zip)
{
	/* Test that making a 7-Zip archive file by default compression
	 * in whatever compressions are supported on the running platform. */
	test_basic(NULL);
	/* Test that making a 7-Zip archive file without empty files. */
	test_basic2(NULL);
}

DEFINE_TEST(test_write_format_7zip_basic_bzip2)
{
	/* Test that making a 7-Zip archive file with bzip2 compression. */
	test_basic("bzip2");
}

DEFINE_TEST(test_write_format_7zip_basic_copy)
{
	/* Test that making a 7-Zip archive file without compression. */
	test_basic("copy");
}

DEFINE_TEST(test_write_format_7zip_basic_deflate)
{
	/* Test that making a 7-Zip archive file with deflate compression. */
	test_basic("deflate");
}

DEFINE_TEST(test_write_format_7zip_basic_lzma1)
{
	/* Test that making a 7-Zip archive file with lzma1 compression. */
	test_basic("lzma1");
}

DEFINE_TEST(test_write_format_7zip_basic_lzma2)
{
	/* Test that making a 7-Zip archive file with lzma2 compression. */
	test_basic("lzma2");
}

DEFINE_TEST(test_write_format_7zip_basic_ppmd)
{
	/* Test that making a 7-Zip archive file with PPMd compression. */
	test_basic("ppmd");
}

/*
 * The PPMd range encoder keeps a run of pending bytes while a carry is
 * unresolved and releases it all at once.  These 100 bytes, after 60000
 * pseudo-random ones, make it release a run of 95 bytes just before the
 * 61440-byte output buffer fills, which overflows the 32-byte overflow
 * buffer behind it.
 */
static const unsigned char ppmd_long_run_tail[] = {
	0x17, 0x55, 0x89, 0x2e, 0xc2, 0xbf, 0x49, 0x5a, 0xf5, 0xac, 0x20, 0x4b,
	0xed, 0x55, 0x5c, 0x8d, 0x63, 0x3c, 0x15, 0x98, 0xe9, 0x5d, 0x36, 0x37,
	0x35, 0x12, 0xb6, 0x09, 0x45, 0x2f, 0xfe, 0xce, 0x7d, 0x2d, 0xc2, 0x88,
	0x02, 0x0c, 0xe9, 0x72, 0x72, 0xac, 0x29, 0xc4, 0xce, 0x93, 0xb4, 0x8f,
	0x3c, 0xd8, 0x0a, 0xce, 0x8e, 0xd7, 0xee, 0x13, 0x5c, 0x54, 0x99, 0xc3,
	0x03, 0x97, 0x7b, 0xea, 0xa1, 0x02, 0xd7, 0xf7, 0xd6, 0xdd, 0x58, 0xcd,
	0x05, 0xbf, 0x3c, 0xc7, 0xcc, 0x58, 0x99, 0xa5, 0x55, 0x56, 0x2e, 0x97,
	0xca, 0x05, 0xb8, 0x00, 0xb0, 0x37, 0x2f, 0x75, 0x21, 0x03, 0x53, 0x8d,
	0xce, 0xc7, 0x5f, 0x01,
};

DEFINE_TEST(test_write_format_7zip_ppmd_long_run)
{
	const size_t prefix = 60000;
	const size_t size = prefix + sizeof(ppmd_long_run_tail) + 8;
	struct archive_entry *ae;
	struct archive *a;
	unsigned char *data, *rbuff;
	char *buff;
	size_t buffsize = size + 4096;
	size_t used, i;
	ssize_t r = 0;
	uint32_t x = 12345;

	data = malloc(size);
	rbuff = malloc(size);
	buff = malloc(buffsize);
	assert(data != NULL && rbuff != NULL && buff != NULL);
	for (i = 0; i < prefix; i++) {
		x = x * 1103515245U + 12345U;
		data[i] = (unsigned char)(x >> 24);
	}
	memcpy(data + prefix, ppmd_long_run_tail, sizeof(ppmd_long_run_tail));
	memcpy(data + prefix + sizeof(ppmd_long_run_tail), "ABCDEFGH", 8);

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_7zip(a));
	if (ARCHIVE_OK != archive_write_set_format_option(a, "7zip",
	    "compression", "ppmd")) {
		skipping("ppmd writing not fully supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_write_free(a));
		free(buff);
		free(rbuff);
		free(data);
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, size);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(size, archive_write_data(a, data, size));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Read the data back. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assertEqualInt(size, archive_entry_size(ae));
	for (i = 0; i < size; i += r) {
		r = archive_read_data(a, rbuff + i, size - i);
		if (r <= 0)
			break;
	}
	assertEqualInt(size, i);
	assertEqualMem(rbuff, data, size);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
	free(rbuff);
	free(data);
}

DEFINE_TEST(test_write_format_7zip_basic_zstd)
{
	/* Test that making a 7-Zip archive file with zstandard compression. */
	test_basic("zstd");
}

DEFINE_TEST(test_write_format_7zip_unregister)
{
	struct archive *a;
	struct archive_write *aw;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_7zip(a));

	aw = (struct archive_write *)a;
	assert(aw->format_data != NULL);
	assert(aw->format_free != NULL);

	assertEqualIntA(a, ARCHIVE_OK,
	    __archive_write_unregister_format(aw));

	assert(aw->format_data == NULL);
	assert(aw->format_name == NULL);
	assert(aw->format_init == NULL);
	assert(aw->format_options == NULL);
	assert(aw->format_finish_entry == NULL);
	assert(aw->format_write_header == NULL);
	assert(aw->format_write_data == NULL);
	assert(aw->format_close == NULL);
	assert(aw->format_free == NULL);
	assertEqualInt(0, aw->archive.archive_format);
	assert(aw->archive.archive_format_name == NULL);

	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_format_option(a, NULL, "compression",
	        "store"));

	assertEqualIntA(a, ARCHIVE_OK,
	    __archive_write_unregister_format(aw));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}
