/*-
 * Copyright (c) 2026 Witt Kung
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

/*
 * Test ARCHIVE_EXTRACT_PREALLOCATE functionality in archive_write_disk.
 */

DEFINE_TEST(test_write_disk_preallocate_regular)
{
	struct archive *ad;
	struct archive_entry *ae;
	struct stat st;
	char data[1024];
	size_t i;
	const int filesize = 64 * 1024; /* 64KB */

	for (i = 0; i < sizeof(data); i++)
		data[i] = (char)(i & 0xff);

	assert((ad = archive_write_disk_new()) != NULL);
	assertEqualInt(ARCHIVE_OK,
	    archive_write_disk_set_options(ad, ARCHIVE_EXTRACT_PREALLOCATE));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test_prealloc_regular.bin");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, filesize);

	assertEqualIntA(ad, ARCHIVE_OK, archive_write_header(ad, ae));

	for (i = 0; i < (size_t)filesize; i += sizeof(data)) {
		assertEqualInt(sizeof(data),
		    archive_write_data(ad, data, sizeof(data)));
	}

	assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
	archive_entry_free(ae);

	/* Verify file size and existence on disk */
	assertEqualInt(0, stat("test_prealloc_regular.bin", &st));
	assertEqualInt(filesize, (int)st.st_size);
}

DEFINE_TEST(test_write_disk_preallocate_zero_byte)
{
	struct archive *ad;
	struct archive_entry *ae;
	struct stat st;

	assert((ad = archive_write_disk_new()) != NULL);
	assertEqualInt(ARCHIVE_OK,
	    archive_write_disk_set_options(ad, ARCHIVE_EXTRACT_PREALLOCATE));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test_prealloc_zero.bin");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 0);

	assertEqualIntA(ad, ARCHIVE_OK, archive_write_header(ad, ae));
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
	archive_entry_free(ae);

	assertEqualInt(0, stat("test_prealloc_zero.bin", &st));
	assertEqualInt(0, (int)st.st_size);
}

DEFINE_TEST(test_write_disk_preallocate_dir_symlink)
{
	struct archive *ad;
	struct archive_entry *ae;
	struct stat st;

	assert((ad = archive_write_disk_new()) != NULL);
	assertEqualInt(ARCHIVE_OK,
	    archive_write_disk_set_options(ad, ARCHIVE_EXTRACT_PREALLOCATE));

	/* Directory with PREALLOCATE flag should succeed smoothly */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test_prealloc_dir");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);

	assertEqualIntA(ad, ARCHIVE_OK, archive_write_header(ad, ae));
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	assertEqualInt(0, stat("test_prealloc_dir", &st));
	assert(S_ISDIR(st.st_mode));

#if !defined(_WIN32) || defined(__CYGWIN__)
	/* Symlink with PREALLOCATE flag should succeed smoothly */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test_prealloc_symlink");
	archive_entry_set_mode(ae, AE_IFLNK | 0777);
	archive_entry_set_symlink(ae, "test_prealloc_dir");

	assertEqualIntA(ad, ARCHIVE_OK, archive_write_header(ad, ae));
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	assertEqualInt(0, lstat("test_prealloc_symlink", &st));
	assert(S_ISLNK(st.st_mode));
#endif

	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
}

DEFINE_TEST(test_write_disk_preallocate_sparse)
{
	struct archive *ad;
	struct archive_entry *ae;
	struct stat st;
	char data[1024];
	size_t i;
	const int filesize = 64 * 1024; /* 64KB */

	for (i = 0; i < sizeof(data); i++)
		data[i] = (char)(i & 0xff);

	assert((ad = archive_write_disk_new()) != NULL);
	assertEqualInt(ARCHIVE_OK,
	    archive_write_disk_set_options(ad,
	    ARCHIVE_EXTRACT_PREALLOCATE | ARCHIVE_EXTRACT_SPARSE));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test_prealloc_sparse.bin");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, filesize);

	assertEqualIntA(ad, ARCHIVE_OK, archive_write_header(ad, ae));

	for (i = 0; i < (size_t)filesize; i += sizeof(data)) {
		assertEqualInt(sizeof(data),
		    archive_write_data(ad, data, sizeof(data)));
	}

	assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
	archive_entry_free(ae);

	/* File should be written correctly despite mixed flags */
	assertEqualInt(0, stat("test_prealloc_sparse.bin", &st));
	assertEqualInt(filesize, (int)st.st_size);
}

/*
 * Small files (< 64KB) should automatically bypass pre-allocation
 * to avoid per-file syscall overhead.  Verify that extraction still
 * succeeds and produces the correct data.
 */
DEFINE_TEST(test_write_disk_preallocate_small_file)
{
	struct archive *ad;
	struct archive_entry *ae;
	struct stat st;
	char data[1024];
	size_t i;
	const int filesize = 32 * 1024; /* 32KB — below 64KB threshold */

	for (i = 0; i < sizeof(data); i++)
		data[i] = (char)(i & 0xff);

	assert((ad = archive_write_disk_new()) != NULL);
	assertEqualInt(ARCHIVE_OK,
	    archive_write_disk_set_options(ad, ARCHIVE_EXTRACT_PREALLOCATE));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test_prealloc_small.bin");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, filesize);

	assertEqualIntA(ad, ARCHIVE_OK, archive_write_header(ad, ae));

	for (i = 0; i < (size_t)filesize; i += sizeof(data)) {
		assertEqualInt(sizeof(data),
		    archive_write_data(ad, data, sizeof(data)));
	}

	assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
	archive_entry_free(ae);

	/* Verify file was correctly written despite bypassing preallocation */
	assertEqualInt(0, stat("test_prealloc_small.bin", &st));
	assertEqualInt(filesize, (int)st.st_size);
}
