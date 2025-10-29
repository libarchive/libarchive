/*-
 * Copyright (c) 2025 libarchive contributors
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
 * Test for Issue #1910: Solid multi-volume RAR5 archives fail with checksum errors
 *
 * Bug Description:
 * When extracting a solid RAR5 archive that is split across multiple volumes,
 * libarchive fails with a checksum error. The command that triggers it:
 *   rar a -m5 -s -t -v16k archive.rar files...
 *
 * Root Cause:
 * Lines 1846-1852 in archive_read_support_format_rar5.c incorrectly reject
 * the first solid file in a split archive when window_buf is NULL. Instead of
 * initializing the window buffer (as should happen), the code returns an error.
 *
 * The problematic code is:
 *   if(rar->cstate.window_buf == NULL) {
 *       return ARCHIVE_FATAL;
 *   }
 *
 * This should instead initialize window_buf when it's NULL and we're starting
 * to process a solid file in a new volume.
 *
 * Expected Behavior:
 * - Archive should extract without checksum errors
 * - All files should extract with correct content
 * - The solid compression dictionary should be properly initialized when
 *   transitioning between volumes
 *
 * Test Strategy:
 * 1. Create a solid RAR5 archive split across multiple 16KB volumes
 * 2. Extract all files from the archive
 * 3. Verify that no checksum errors occur
 * 4. Verify that extracted file content matches expected patterns
 *
 * This test will FAIL before the fix (checksum error) and PASS after the fix.
 */

/* Expected content patterns for verification */
static const char file1_expected[] =
	"This is test file 1 for Issue #1910.\n"
	"Solid+Split RAR archives should work without checksum errors.\n"
	"This content should extract correctly.\n";

static const char file3_expected[] =
	"Third file in the solid archive.\n"
	"The solid compression reuses the dictionary from previous files.\n"
	"This is where Issue #1910 manifests: when window_buf is NULL\n"
	"on the first solid file in a split volume, the code incorrectly\n"
	"returns an error instead of initializing the buffer.\n";

/*
 * Verify file2.bin size: should be 100KB of random data
 * (We can't verify content since it's random, just verify size)
 */
static int
verify_file2_bin(const uint8_t *data, size_t size)
{
	(void)data; /* Not verifying content, just size */

	if (size != 100 * 1024) {
		/* Expected size is 102400 bytes (100 KB) */
		return 0;
	}

	return 1;
}

/*
 * Verify file4.bin size: should be 50KB of random data
 * (We can't verify content since it's random, just verify size)
 */
static int
verify_file4_bin(const uint8_t *data, size_t size)
{
	(void)data; /* Not verifying content, just size */

	if (size != 50 * 1024) {
		/* Expected size is 51200 bytes (50 KB) */
		return 0;
	}

	return 1;
}

DEFINE_TEST(test_read_format_rar5_solid_split)
{
	struct archive_entry *ae;
	struct archive *a;
	const char* reffiles[] = {
		"test_read_format_rar5_solid_split.part01.rar",
		"test_read_format_rar5_solid_split.part02.rar",
		"test_read_format_rar5_solid_split.part03.rar",
		"test_read_format_rar5_solid_split.part04.rar",
		"test_read_format_rar5_solid_split.part05.rar",
		"test_read_format_rar5_solid_split.part06.rar",
		"test_read_format_rar5_solid_split.part07.rar",
		"test_read_format_rar5_solid_split.part08.rar",
		"test_read_format_rar5_solid_split.part09.rar",
		"test_read_format_rar5_solid_split.part10.rar",
		"test_read_format_rar5_solid_split.part11.rar",
		"test_read_format_rar5_solid_split.part12.rar",
		"test_read_format_rar5_solid_split.part13.rar",
		"test_read_format_rar5_solid_split.part14.rar",
		"test_read_format_rar5_solid_split.part15.rar",
		"test_read_format_rar5_solid_split.part16.rar",
		"test_read_format_rar5_solid_split.part17.rar",
		"test_read_format_rar5_solid_split.part18.rar",
		"test_read_format_rar5_solid_split.part19.rar",
		"test_read_format_rar5_solid_split.part20.rar",
		NULL
	};
	uint8_t *file_data;
	la_ssize_t file_size;
	int ret;

	extract_reference_files(reffiles);

	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_filter_all(a));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_open_filenames(a, reffiles, 10240));

	/*
	 * Extract and verify file2.bin (returned first)
	 * This file is large (100KB) and spans multiple volumes.
	 * This is where Issue #1910 commonly manifests.
	 */
	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);
	assertEqualString("file2.bin", archive_entry_pathname(ae));

	file_size = archive_entry_size(ae);

	file_data = malloc(file_size);
	assertA(file_data != NULL);

	/* Critical test: Should extract without checksum error */
	assertEqualIntA(a, file_size, archive_read_data(a, file_data, file_size));
	assertA(verify_file2_bin(file_data, file_size));
	free(file_data);

	/*
	 * Extract and verify file4.bin (returned second)
	 */
	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);
	assertEqualString("file4.bin", archive_entry_pathname(ae));

	file_size = archive_entry_size(ae);

	file_data = malloc(file_size);
	assertA(file_data != NULL);

	assertEqualIntA(a, file_size, archive_read_data(a, file_data, file_size));
	assertA(verify_file4_bin(file_data, file_size));
	free(file_data);

	/*
	 * Extract and verify file1.txt (returned third)
	 */
	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);
	assertEqualString("file1.txt", archive_entry_pathname(ae));

	file_size = archive_entry_size(ae);
	assertEqualInt(sizeof(file1_expected) - 1, file_size);

	file_data = malloc(file_size);
	assertA(file_data != NULL);

	/* This read should NOT produce a checksum error (Issue #1910) */
	assertEqualIntA(a, file_size, archive_read_data(a, file_data, file_size));
	assertEqualMem(file_data, file1_expected, file_size);
	free(file_data);

	/*
	 * Extract and verify file3.txt (returned last)
	 */
	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);
	assertEqualString("file3.txt", archive_entry_pathname(ae));

	file_size = archive_entry_size(ae);
	assertEqualInt(sizeof(file3_expected) - 1, file_size);

	file_data = malloc(file_size);
	assertA(file_data != NULL);

	assertEqualIntA(a, file_size, archive_read_data(a, file_data, file_size));
	assertEqualMem(file_data, file3_expected, file_size);
	free(file_data);

	/* Should be at end of archive */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Clean up */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Test variant: Skip all files without extracting data
 * This tests that header reading works correctly even when data is not read
 */
DEFINE_TEST(test_read_format_rar5_solid_split_skip_all)
{
	struct archive_entry *ae;
	struct archive *a;
	const char* reffiles[] = {
		"test_read_format_rar5_solid_split.part01.rar",
		"test_read_format_rar5_solid_split.part02.rar",
		"test_read_format_rar5_solid_split.part03.rar",
		"test_read_format_rar5_solid_split.part04.rar",
		"test_read_format_rar5_solid_split.part05.rar",
		"test_read_format_rar5_solid_split.part06.rar",
		"test_read_format_rar5_solid_split.part07.rar",
		"test_read_format_rar5_solid_split.part08.rar",
		"test_read_format_rar5_solid_split.part09.rar",
		"test_read_format_rar5_solid_split.part10.rar",
		"test_read_format_rar5_solid_split.part11.rar",
		"test_read_format_rar5_solid_split.part12.rar",
		"test_read_format_rar5_solid_split.part13.rar",
		"test_read_format_rar5_solid_split.part14.rar",
		"test_read_format_rar5_solid_split.part15.rar",
		"test_read_format_rar5_solid_split.part16.rar",
		"test_read_format_rar5_solid_split.part17.rar",
		"test_read_format_rar5_solid_split.part18.rar",
		"test_read_format_rar5_solid_split.part19.rar",
		"test_read_format_rar5_solid_split.part20.rar",
		NULL
	};
	int ret;

	extract_reference_files(reffiles);

	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_filter_all(a));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_open_filenames(a, reffiles, 10240));

	/* Just read headers, don't extract data - order: file2, file4, file1, file3 */
	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);
	assertEqualString("file2.bin", archive_entry_pathname(ae));

	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);
	assertEqualString("file4.bin", archive_entry_pathname(ae));

	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);
	assertEqualString("file1.txt", archive_entry_pathname(ae));

	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);
	assertEqualString("file3.txt", archive_entry_pathname(ae));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Test variant: Extract only middle file (file2.bin)
 * This specifically targets Issue #1910 by extracting only the file
 * that spans multiple volumes
 */
DEFINE_TEST(test_read_format_rar5_solid_split_extract_middle)
{
	struct archive_entry *ae;
	struct archive *a;
	const char* reffiles[] = {
		"test_read_format_rar5_solid_split.part01.rar",
		"test_read_format_rar5_solid_split.part02.rar",
		"test_read_format_rar5_solid_split.part03.rar",
		"test_read_format_rar5_solid_split.part04.rar",
		"test_read_format_rar5_solid_split.part05.rar",
		"test_read_format_rar5_solid_split.part06.rar",
		"test_read_format_rar5_solid_split.part07.rar",
		"test_read_format_rar5_solid_split.part08.rar",
		"test_read_format_rar5_solid_split.part09.rar",
		"test_read_format_rar5_solid_split.part10.rar",
		"test_read_format_rar5_solid_split.part11.rar",
		"test_read_format_rar5_solid_split.part12.rar",
		"test_read_format_rar5_solid_split.part13.rar",
		"test_read_format_rar5_solid_split.part14.rar",
		"test_read_format_rar5_solid_split.part15.rar",
		"test_read_format_rar5_solid_split.part16.rar",
		"test_read_format_rar5_solid_split.part17.rar",
		"test_read_format_rar5_solid_split.part18.rar",
		"test_read_format_rar5_solid_split.part19.rar",
		"test_read_format_rar5_solid_split.part20.rar",
		NULL
	};
	uint8_t *file_data;
	la_ssize_t file_size;
	int ret;

	extract_reference_files(reffiles);

	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_filter_all(a));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_open_filenames(a, reffiles, 10240));

	/* Extract file2.bin (returned first - the critical test case) */
	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);
	assertEqualString("file2.bin", archive_entry_pathname(ae));

	file_size = archive_entry_size(ae);
	file_data = malloc(file_size);
	assertA(file_data != NULL);

	/* This is the critical test for Issue #1910 */
	assertEqualIntA(a, file_size, archive_read_data(a, file_data, file_size));
	assertA(verify_file2_bin(file_data, file_size));
	free(file_data);

	/* Skip remaining files (file4, file1, file3) */
	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);

	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);

	ret = archive_read_next_header(a, &ae);
	assertEqualIntA(a, ARCHIVE_OK, ret);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
