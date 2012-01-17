/*-
 * Copyright (c) 2012 Michihiro NAKAJIMA
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

static void
test_newer_time(void)
{
	struct archive_entry *ae;
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}

	assertEqualIntA(m, 0, archive_matching_newer_mtime(m, 7880, 0));
	assertEqualIntA(m, 0, archive_matching_newer_ctime(m, 7880, 0));

	archive_entry_copy_pathname(ae, "file1");
	archive_entry_set_mtime(ae, 7880, 0);
	archive_entry_set_ctime(ae, 7880, 0);
	failure("Both Its mtime and ctime should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));
	archive_entry_set_mtime(ae, 7879, 999);
	archive_entry_set_ctime(ae, 7879, 999);
	failure("Both Its mtime and ctime should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	archive_entry_set_mtime(ae, 7881, 0);
	archive_entry_set_ctime(ae, 7881, 0);
	failure("Both Its mtime and ctime should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	archive_entry_set_mtime(ae, 7880, 1);
	archive_entry_set_ctime(ae, 7880, 0);
	failure("Its mtime should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	archive_entry_set_mtime(ae, 7880, 0);
	archive_entry_set_ctime(ae, 7880, 1);
	failure("Its ctime should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Clean up. */
	archive_entry_free(ae);
	archive_matching_free(m);
}

static void
test_newer_than_file_mbs(void)
{
	struct archive *a;
	struct archive_entry *ae;
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}
	if (!assert((a = archive_read_disk_new()) != NULL)) {
		archive_matching_free(m);
		archive_entry_free(ae);
		return;
	}

	assertMakeDir("test_newer_than_file_mbs", 0777);
	assertChdir("test_newer_than_file_mbs");

	assertMakeFile("old", 0666, "old");
	sleepUntilAfter(time(NULL));
	assertMakeFile("mid", 0666, "mid");
	sleepUntilAfter(time(NULL));
	assertMakeFile("new", 0666, "new");

	/*
	 * Test 'newer mtime than'.
	 */
	assertEqualIntA(m, 0, archive_matching_newer_mtime_than(m, "mid"));

	/* Verify 'old' file. */
	archive_entry_copy_pathname(ae, "old");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'mid' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "mid");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'new' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "new");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	/*
	 * Test 'newer ctime than'.
	 */
	archive_matching_free(m);
	if (!assert((m = archive_matching_new()) != NULL)) {
		assertChdir("..");
		archive_entry_free(ae);
		archive_read_free(a);
		return;
	}
	assertEqualIntA(m, 0, archive_matching_newer_ctime_than(m, "mid"));

	/* Verify 'old' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "old");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'mid' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "mid");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'new' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "new");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	assertChdir("..");
	/* Clean up. */
	archive_read_free(a);
	archive_entry_free(ae);
	archive_matching_free(m);
}

static void
test_newer_than_file_wcs(void)
{
	struct archive *a;
	struct archive_entry *ae;
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}
	if (!assert((a = archive_read_disk_new()) != NULL)) {
		archive_matching_free(m);
		archive_entry_free(ae);
		return;
	}

	assertMakeDir("test_newer_than_file_wcs", 0777);
	assertChdir("test_newer_than_file_wcs");

	assertMakeFile("old", 0666, "old");
	sleepUntilAfter(time(NULL));
	assertMakeFile("mid", 0666, "mid");
	sleepUntilAfter(time(NULL));
	assertMakeFile("new", 0666, "new");

	/*
	 * Test 'newer mtime than'.
	 */
	assertEqualIntA(m, 0, archive_matching_newer_mtime_than_w(m, L"mid"));

	/* Verify 'old' file. */
	archive_entry_copy_pathname(ae, "old");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'mid' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "mid");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'new' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "new");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	/*
	 * Test 'newer ctime than'.
	 */
	archive_matching_free(m);
	if (!assert((m = archive_matching_new()) != NULL)) {
		assertChdir("..");
		archive_entry_free(ae);
		archive_read_free(a);
		return;
	}
	assertEqualIntA(m, 0, archive_matching_newer_ctime_than_w(m, L"mid"));

	/* Verify 'old' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "old");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'mid' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "mid");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'new' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "new");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	assertChdir("..");
	/* Clean up. */
	archive_read_free(a);
	archive_entry_free(ae);
	archive_matching_free(m);
}

static void
test_older_time(void)
{
	struct archive_entry *ae;
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}

	assertEqualIntA(m, 0, archive_matching_older_mtime(m, 7880, 0));
	assertEqualIntA(m, 0, archive_matching_older_ctime(m, 7880, 0));

	archive_entry_copy_pathname(ae, "file1");
	archive_entry_set_mtime(ae, 7880, 0);
	archive_entry_set_ctime(ae, 7880, 0);
	failure("Both Its mtime and ctime should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));
	archive_entry_set_mtime(ae, 7879, 999);
	archive_entry_set_ctime(ae, 7879, 999);
	failure("Both Its mtime and ctime should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	archive_entry_set_mtime(ae, 7881, 0);
	archive_entry_set_ctime(ae, 7881, 0);
	failure("Both Its mtime and ctime should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	archive_entry_set_mtime(ae, 7880, 1);
	archive_entry_set_ctime(ae, 7879, 0);
	failure("Its mtime should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	archive_entry_set_mtime(ae, 7879, 0);
	archive_entry_set_ctime(ae, 7880, 1);
	failure("Its ctime should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Clean up. */
	archive_entry_free(ae);
	archive_matching_free(m);
}

static void
test_older_than_file_mbs(void)
{
	struct archive *a;
	struct archive_entry *ae;
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}
	if (!assert((a = archive_read_disk_new()) != NULL)) {
		archive_matching_free(m);
		archive_entry_free(ae);
		return;
	}

	assertMakeDir("test_older_than_file_mbs", 0777);
	assertChdir("test_older_than_file_mbs");

	assertMakeFile("old", 0666, "old");
	sleepUntilAfter(time(NULL));
	assertMakeFile("mid", 0666, "mid");
	sleepUntilAfter(time(NULL));
	assertMakeFile("new", 0666, "new");

	/*
	 * Test 'older mtime than'.
	 */
	assertEqualIntA(m, 0, archive_matching_older_mtime_than(m, "mid"));

	/* Verify 'old' file. */
	archive_entry_copy_pathname(ae, "old");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	/* Verify 'mid' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "mid");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'new' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "new");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/*
	 * Test 'older ctime than'.
	 */
	archive_matching_free(m);
	if (!assert((m = archive_matching_new()) != NULL)) {
		assertChdir("..");
		archive_entry_free(ae);
		archive_read_free(a);
		return;
	}
	assertEqualIntA(m, 0, archive_matching_older_ctime_than(m, "mid"));

	/* Verify 'old' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "old");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	/* Verify 'mid' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "mid");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'new' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "new");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	assertChdir("..");
	/* Clean up. */
	archive_read_free(a);
	archive_entry_free(ae);
	archive_matching_free(m);
}

static void
test_older_than_file_wcs(void)
{
	struct archive *a;
	struct archive_entry *ae;
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}
	if (!assert((a = archive_read_disk_new()) != NULL)) {
		archive_matching_free(m);
		archive_entry_free(ae);
		return;
	}

	assertMakeDir("test_older_than_file_wcs", 0777);
	assertChdir("test_older_than_file_wcs");

	assertMakeFile("old", 0666, "old");
	sleepUntilAfter(time(NULL));
	assertMakeFile("mid", 0666, "mid");
	sleepUntilAfter(time(NULL));
	assertMakeFile("new", 0666, "new");

	/*
	 * Test 'older mtime than'.
	 */
	assertEqualIntA(m, 0, archive_matching_older_mtime_than_w(m, L"mid"));

	/* Verify 'old' file. */
	archive_entry_copy_pathname(ae, "old");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	/* Verify 'mid' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "mid");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'new' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "new");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/*
	 * Test 'older ctime than'.
	 */
	archive_matching_free(m);
	if (!assert((m = archive_matching_new()) != NULL)) {
		assertChdir("..");
		archive_entry_free(ae);
		archive_read_free(a);
		return;
	}
	assertEqualIntA(m, 0, archive_matching_older_ctime_than_w(m, L"mid"));

	/* Verify 'old' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "old");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	/* Verify 'mid' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "mid");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	/* Verify 'new' file. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "new");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_disk_entry_from_file(a, ae, -1, NULL));
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	assertChdir("..");
	/* Clean up. */
	archive_read_free(a);
	archive_entry_free(ae);
	archive_matching_free(m);
}

static void
excluded(struct archive *m)
{
	struct archive_entry *ae;

	if (!assert((ae = archive_entry_new()) != NULL))
		return;

	archive_entry_copy_pathname(ae, "file1");
	archive_entry_set_mtime(ae, 7879, 999);
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));
	archive_entry_set_mtime(ae, 7880, 0);
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));
	archive_entry_set_mtime(ae, 7880, 1);
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	archive_entry_copy_pathname(ae, "file2");
	archive_entry_set_mtime(ae, 7879, 999);
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));
	archive_entry_set_mtime(ae, 7880, 0);
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));
	archive_entry_set_mtime(ae, 7880, 1);
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));

	archive_entry_copy_pathname(ae, "file3");
	archive_entry_set_mtime(ae, 7879, 999);
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));
	archive_entry_set_mtime(ae, 7880, 0);
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));
	archive_entry_set_mtime(ae, 7880, 1);
	failure("It should be excluded");
	assertEqualInt(1, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(1, archive_matching_excluded_ae(m, ae));

	archive_entry_copy_pathname(ae, "file4");
	archive_entry_set_mtime(ae, 7879, 999);
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));
	archive_entry_set_mtime(ae, 7880, 0);
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));
	archive_entry_set_mtime(ae, 7880, 1);
	failure("It should not be excluded");
	assertEqualInt(0, archive_matching_time_excluded_ae(m, ae));
	assertEqualInt(0, archive_matching_excluded_ae(m, ae));


	/* Clean up. */
	archive_entry_free(ae);
}

static void
test_newer_pathname_mbs(void)
{
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;

	assertEqualIntA(m, 0,
	    archive_matching_pathname_newer_mtime(m, "file1", 7880, 0));
	assertEqualIntA(m, 0,
	    archive_matching_pathname_newer_mtime(m, "file2", 1, 0));
	assertEqualIntA(m, 0,
	    archive_matching_pathname_newer_mtime(m, "file3", 99999, 0));

	excluded(m);

	/* Clean up. */
	archive_matching_free(m);
}

static void
test_newer_pathname_wcs(void)
{
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;

	assertEqualIntA(m, 0,
	    archive_matching_pathname_newer_mtime_w(m, L"file1", 7880, 0));
	assertEqualIntA(m, 0,
	    archive_matching_pathname_newer_mtime_w(m, L"file2", 1, 0));
	assertEqualIntA(m, 0,
	    archive_matching_pathname_newer_mtime_w(m, L"file3", 99999, 0));

	excluded(m);

	/* Clean up. */
	archive_matching_free(m);
}

static void
test_newer_archive_entry(void)
{
	struct archive_entry *ae;
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}

	archive_entry_copy_pathname(ae, "file1");
	archive_entry_set_mtime(ae, 7880, 0);
	assertEqualIntA(m, 0, archive_matching_pathname_newer_mtime_ae(m, ae));
	archive_entry_copy_pathname(ae, "file2");
	archive_entry_set_mtime(ae, 1, 0);
	assertEqualIntA(m, 0, archive_matching_pathname_newer_mtime_ae(m, ae));
	archive_entry_copy_pathname(ae, "file3");
	archive_entry_set_mtime(ae, 99999, 0);
	assertEqualIntA(m, 0, archive_matching_pathname_newer_mtime_ae(m, ae));

	excluded(m);

	/* Clean up. */
	archive_entry_free(ae);
	archive_matching_free(m);
}

DEFINE_TEST(test_archive_matching_time)
{
	test_newer_time();
	test_newer_than_file_mbs();
	test_newer_than_file_wcs();
	test_older_time();
	test_older_than_file_mbs();
	test_older_than_file_wcs();
	test_newer_pathname_mbs();
	test_newer_pathname_wcs();
	test_newer_archive_entry();
}
