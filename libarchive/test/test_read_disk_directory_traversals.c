/*-
 * Copyright (c) 2010 Michihiro NAKAJIMA
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

#if defined(_WIN32) && !defined(__CYGWIN__)
# if !defined(__BORLANDC__)
#  define getcwd _getcwd
# endif
#endif

DEFINE_TEST(test_read_disk_directory_traversals)
{
	struct archive *a;
	struct archive_entry *ae;
	const void *p;
	char *initial_cwd, *cwd;
	size_t size;
	int64_t offset;
	int file_count;

	assertMakeDir("dir1", 0755);
	assertMakeFile("dir1/file1", 0644, "0123456789");
	assertMakeFile("dir1/file2", 0644, "hello world");
	assertMakeDir("dir1/sub1", 0755);
	assertMakeFile("dir1/sub1/file1", 0644, "0123456789");
	assertMakeDir("dir1/sub2", 0755);
	assertMakeFile("dir1/sub2/file1", 0644, "0123456789");
	assertMakeFile("dir1/sub2/file2", 0644, "0123456789");
	assertMakeDir("dir1/sub2/sub1", 0755);
	assertMakeDir("dir1/sub2/sub2", 0755);
	assertMakeDir("dir1/sub2/sub3", 0755);
	assertMakeFile("dir1/sub2/sub3/file", 0644, "xyz");
	file_count = 12;

	assert((ae = archive_entry_new()) != NULL);
	assert((a = archive_read_disk_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "dir1"));

	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae), "dir1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualInt(memcmp(p, "0123456789", 10), 0);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 11);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 11);
			assertEqualInt((int)offset, 0);
			assertEqualInt(memcmp(p, "hello world", 11), 0);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 11);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub1/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualInt(memcmp(p, "0123456789", 10), 0);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/file1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualInt(memcmp(p, "0123456789", 10), 0);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/file2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 10);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 10);
			assertEqualInt((int)offset, 0);
			assertEqualInt(memcmp(p, "0123456789", 10), 0);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 10);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/sub1") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/sub2") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/sub3") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
		} else if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub2/sub3/file") == 0) {
			assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
			assertEqualInt(archive_entry_size(ae), 3);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 3);
			assertEqualInt((int)offset, 0);
			assertEqualInt(memcmp(p, "xyz", 3), 0);
			assertEqualInt(ARCHIVE_EOF,
			    archive_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
			assertEqualInt((int)offset, 3);
		}
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));

	/* Destroy the archive. */
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * Test that call archive_read_disk_open with a regular file.
	 */
	assert((a = archive_read_disk_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "dir1/file1"));

	/* dir1/file1 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
	assertEqualString(archive_entry_pathname(ae), "dir1/file1");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
	assertEqualInt(archive_entry_size(ae), 10);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_data_block(a, &p, &size, &offset));
	assertEqualInt((int)size, 10);
	assertEqualInt((int)offset, 0);
	assertEqualInt(memcmp(p, "0123456789", 10), 0);

	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));

	/* Destroy the archive. */
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));


#if defined(_WIN32) && !defined(__CYGWIN__)
	/*
	 * Test for wildcard '*' or '?'
	 */
	assert((a = archive_read_disk_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "dir1/*1"));

	/* dir1/file1 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
	assertEqualString(archive_entry_pathname(ae), "dir1/file1");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
	assertEqualInt(archive_entry_size(ae), 10);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_data_block(a, &p, &size, &offset));
	assertEqualInt((int)size, 10);
	assertEqualInt((int)offset, 0);
	assertEqualInt(memcmp(p, "0123456789", 10), 0);

	/* dir1/sub1 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
	assertEqualString(archive_entry_pathname(ae), "dir1/sub1");
	assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);

	/* Descend into the current object */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_descend(a));

	/* dir1/sub1/file1 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header2(a, ae));
	assertEqualString(archive_entry_pathname(ae), "dir1/sub1/file1");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
	assertEqualInt(archive_entry_size(ae), 10);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_data_block(a, &p, &size, &offset));
	assertEqualInt((int)size, 10);
	assertEqualInt((int)offset, 0);
	assertEqualInt(memcmp(p, "0123456789", 10), 0);

	/* There is no entry. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header2(a, ae));

	/* Destroy the archive. */
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#endif

	/*
	 * We should be on the initial directory where we performed
	 * archive_read_disk_new() after we perfome archive_read_free()
	 *  even if we broke off the directory traversals.
	 */

	/* Save current working directory. */
	initial_cwd = getcwd(NULL, 0);

	assert((a = archive_read_disk_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "dir1"));

	/* Step in a deep directory. */
	file_count = 12;
	while (file_count--) {
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_next_header2(a, ae));
		if (strcmp(archive_entry_pathname(ae),
		    "dir1/sub1/file1") == 0)
			/*
			 * We are on an another directory at this time.
			 */
			break;
		if (archive_entry_filetype(ae) == AE_IFDIR) {
			/* Descend into the current object */
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_disk_descend(a));
		}
	}
	/* Destroy the archive. */
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* We should be on the initial working directory. */
	failure(
	    "Current working directory does not return to the initial"
	    "directory");
	cwd = getcwd(NULL, 0);
	assertEqualString(initial_cwd, cwd);
	free(initial_cwd);
	free(cwd);

	archive_entry_free(ae);
}
