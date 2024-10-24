/*-
 * Copyright (c) 2024 Yang Zhou
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
test_with_hdrcharset(const char *charset)
{
	static const char *raw_path = "dir_stored\\dir1/file";
	static const char *replaced = "dir_stored/dir1/file";
	struct archive *a;
	size_t used;
	size_t buffsize = 1000000;
	char *buff;

	buff = malloc(buffsize);

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	if (charset != NULL) {
		assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_option(a, "zip", "hdrcharset", charset));
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, buffsize, &used));

	/*
	 * Write a file with mixed '/' and '\'
	 */
	struct archive_entry *ae;
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 10);
	archive_entry_copy_pathname(ae, raw_path);
	archive_entry_set_filetype(ae, AE_IFREG | 0755);
	archive_entry_set_size(ae, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));
	dumpfile("constructed.zip", buff, used);

	/*
	 * Check if the generated archive contains and only contains expected path.
	 * Intentionally avoid using `archive_read_XXX` functions because it silently replaces '\' with '/',
	 * making it difficult to get the exact path written in the archive.
	 */
#if defined(_WIN32) && !defined(__CYGWIN__)
	const char *expected = replaced;
	const char *unexpected = raw_path;
#else
	const char *expected = raw_path;
	const char *unexpected = replaced;
#endif
	int expected_found = 0;
	int unexpected_found = 0;
	size_t len = strlen(raw_path);
	for (char *ptr = buff; ptr < (buff + used - len); ptr++) {
		if (memcmp(ptr, expected, len) == 0)
			++expected_found;
		if (memcmp(ptr, unexpected, len) == 0)
			++unexpected_found;
	}
	failure("should find expected path in both local and central header (charset=%s)", charset);
	assertEqualInt(2, expected_found);
	failure("should not find unexpected path in anywhere (charset=%s)", charset);
	assertEqualInt(0, unexpected_found);

	free(buff);
}

DEFINE_TEST(test_write_format_zip_windows_path_charset)
{
	test_with_hdrcharset(NULL);
#if defined(_WIN32) && !defined(__CYGWIN__) || HAVE_ICONV
	test_with_hdrcharset("ISO-8859-1");
	test_with_hdrcharset("UTF-8");
#endif
}

#define FA_READONLY      (0x00000001U)
#define FA_DIRECTORY     (0x00000010U)
#define FA_NORMAL        (0x00000080U)
#define FA_OTHER_BITS    (0x00003126U)

#define READ_STREAM (0)  /* Least capable. Have to guess file types. */
#define READ_SEEK   (1)  /* Most capable. Retrieve file info from external attributes. */
#define READ_POSIX  (2)  /* Same as SEEK but does not save attributes as `fflags`. */

#define checkFFlags(EXPECT) do { \
	unsigned long set, clear; \
	archive_entry_fflags(ae, &set, &clear); \
	failure("unexpected fflags under %s mode", strmode); \
	if (mode == READ_SEEK) \
		assertEqualIntA(a, EXPECT, set); \
	else \
		assertEqualIntA(a, 0, set); \
} while (0)

static void
verify(const char *buff, size_t used, int mode)
{
	struct archive *a;
	struct archive_entry *ae;
	const char *modes[] = {"STREAM", "SEEK", "POSIX"};
	const char *strmode = modes[mode];

	/* Read the archive in the desired mode. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	if (mode != READ_POSIX)
		assertEqualIntA(a, ARCHIVE_OK, archive_read_set_format_option(a, "zip", "os", "windows"));
	if (mode == READ_STREAM)
		assertEqualIntA(a, ARCHIVE_OK, read_open_memory_minimal(a, buff, used, 7));
	else
		assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, buff, used, 7));

	/* Read a file. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "file", archive_entry_pathname(ae));
	assertEqualIntA(a, AE_IFREG | 0664, archive_entry_mode(ae));
	checkFFlags(FA_NORMAL);

	/* Read a file. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "file2", archive_entry_pathname(ae));
	assertEqualIntA(a, AE_IFREG | 0664, archive_entry_mode(ae));
	checkFFlags(FA_OTHER_BITS);

	/* Read a file. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "file_ro", archive_entry_pathname(ae));
	if (mode == READ_STREAM)
		assertEqualIntA(a, AE_IFREG | 0664, archive_entry_mode(ae));
	else
		assertEqualIntA(a, AE_IFREG | 0444, archive_entry_mode(ae));
	checkFFlags(FA_READONLY);

	/* Read a directory. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "dir/", archive_entry_pathname(ae));
	assertEqualIntA(a, AE_IFDIR | 0775, archive_entry_mode(ae));
	checkFFlags(FA_DIRECTORY);

	/* Read a directory. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualStringA(a, "dir_ro/", archive_entry_pathname(ae));
	if (mode == READ_STREAM)
		assertEqualIntA(a, AE_IFDIR | 0775, archive_entry_mode(ae));
	else
		assertEqualIntA(a, AE_IFDIR | 0555, archive_entry_mode(ae));
	checkFFlags(FA_DIRECTORY | FA_READONLY);

	/* Close out the archive. */
	assertEqualInt(ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}

DEFINE_TEST(test_write_format_zip_windows_file_flags)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t used;
	size_t buffsize = 1000000;
	char *buff;

	buff = malloc(buffsize);

	/* Create a new Windows archive. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_option(a, "zip", "os", "windows"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, buffsize, &used));

	/* Write a file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_fflags(ae, FA_NORMAL, 0);
	assertEqualInt(0, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Write another file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file2");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_fflags(ae, FA_OTHER_BITS, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Write a readonly file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file_ro");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_fflags(ae, FA_READONLY, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Write a directory. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_filetype(ae, AE_IFDIR);
	archive_entry_set_fflags(ae, FA_DIRECTORY, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Write a readonly directory. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir_ro");
	archive_entry_set_filetype(ae, AE_IFDIR);
	archive_entry_set_fflags(ae, FA_DIRECTORY | FA_READONLY, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Close out the archive. */
	assertEqualInt(ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	verify(buff, used, READ_STREAM);
	verify(buff, used, READ_SEEK);
	verify(buff, used, READ_POSIX);

	free(buff);
}
