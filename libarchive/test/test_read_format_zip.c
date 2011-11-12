/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2011 Michihiro NAKAJIMA
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
__FBSDID("$FreeBSD: head/lib/libarchive/test/test_read_format_zip.c 189482 2009-03-07 03:30:35Z kientzle $");

#ifdef HAVE_LIBZ
static const int libz_enabled = 1;
#else
static const int libz_enabled = 0;
#endif

/*
 * The reference file for this has been manually tweaked so that:
 *   * file2 has length-at-end but file1 does not
 *   * file2 has an invalid CRC
 */

static void
test_basic(void)
{
	const char *refname = "test_read_format_zip.zip";
	struct archive_entry *ae;
	struct archive *a;
	char *buff[128];
	const void *pv;
	size_t s;
	int64_t o;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_filter_all(a));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_open_filename(a, refname, 10240));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("dir/", archive_entry_pathname(ae));
	assertEqualInt(1179604249, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_EOF,
	    archive_read_data_block(a, &pv, &s, &o));
	assertEqualInt((int)s, 0);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(1179604289, archive_entry_mtime(ae));
	assertEqualInt(18, archive_entry_size(ae));
	failure("archive_read_data() returns number of bytes read");
	if (libz_enabled) {
		assertEqualInt(18, archive_read_data(a, buff, 19));
		assertEqualMem(buff, "hello\nhello\nhello\n", 18);
	} else {
		assertEqualInt(ARCHIVE_FATAL, archive_read_data(a, buff, 19));
		assertEqualString(archive_error_string(a),
		    "libarchive compiled without deflate support (no libz)");
		assert(archive_errno(a) != 0);
	}
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(1179605932, archive_entry_mtime(ae));
	failure("file2 has length-at-end, so we shouldn't see a valid size");
	assertEqualInt(0, archive_entry_size_is_set(ae));
	if (libz_enabled) {
		failure("file2 has a bad CRC, so read should fail and not change buff");
		memset(buff, 'a', 19);
		assertEqualInt(ARCHIVE_WARN, archive_read_data(a, buff, 19));
		assertEqualMem(buff, "aaaaaaaaaaaaaaaaaaa", 19);
	} else {
		assertEqualInt(ARCHIVE_FATAL, archive_read_data(a, buff, 19));
		assertEqualString(archive_error_string(a),
		    "libarchive compiled without deflate support (no libz)");
		assert(archive_errno(a) != 0);
	}
	/* Verify the number of files read. */
	failure("the archive file has three files");
	assertEqualInt(3, archive_file_count(a));
	assertA(archive_compression(a) == ARCHIVE_COMPRESSION_NONE);
	assertA(archive_format(a) == ARCHIVE_FORMAT_ZIP);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Read Info-ZIP New Unix Extra Field 0x7875 "ux".
 *  Currently stores Unix UID/GID up to 32 bits.
 */
static void
test_info_zip_ux(void)
{
	const char *refname = "test_read_format_zip_ux.zip";
	struct archive_entry *ae;
	struct archive *a;
	char *buff[128];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_filter_all(a));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_open_filename(a, refname, 10240));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(1300668680, archive_entry_mtime(ae));
	assertEqualInt(18, archive_entry_size(ae));
	failure("zip reader should read Info-ZIP New Unix Extra Field");
	assertEqualInt(1001, archive_entry_uid(ae));
	assertEqualInt(1001, archive_entry_gid(ae));
	failure("archive_read_data() returns number of bytes read");
	if (libz_enabled) {
		assertEqualInt(18, archive_read_data(a, buff, 19));
		assertEqualMem(buff, "hello\nhello\nhello\n", 18);
	} else {
		assertEqualInt(ARCHIVE_FATAL, archive_read_data(a, buff, 19));
		assertEqualString(archive_error_string(a),
		    "libarchive compiled without deflate support (no libz)");
		assert(archive_errno(a) != 0);
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify the number of files read. */
	failure("the archive file has just one file");
	assertEqualInt(1, archive_file_count(a));

	assertA(archive_compression(a) == ARCHIVE_COMPRESSION_NONE);
	assertA(archive_format(a) == ARCHIVE_FORMAT_ZIP);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Verify that test_read_extract correctly works with
 * Zip entries that use length-at-end.
 */
static void
test_extract_length_at_end(void)
{
	const char *refname = "test_read_format_zip_length_at_end.zip";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("hello.txt", archive_entry_pathname(ae));
	assert(!archive_entry_size_is_set(ae));
	assertEqualInt(0, archive_entry_size(ae));

	if (libz_enabled) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_extract(a, ae, 0));
		assertFileContents("hello\x0A", 6, "hello.txt");
	} else {
		assertEqualIntA(a, ARCHIVE_FATAL, archive_read_extract(a, ae, 0));
		assertEqualString(archive_error_string(a),
		    "libarchive compiled without deflate support (no libz)");
		assert(archive_errno(a) != 0);
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_zip)
{
	test_basic();
	test_info_zip_ux();
	test_extract_length_at_end();
}
