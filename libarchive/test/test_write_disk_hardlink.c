/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

#if defined(_WIN32) && !defined(__CYGWIN__)
/* Execution bits, Group members bits and others bits do not work. */
#define UMASK 0177
#define E_MASK (~0177)
#else
#define UMASK 022
#define E_MASK (~0)
#endif

/*
 * Exercise hardlink recreation.
 *
 * File permissions are chosen so that the authoritative entry
 * has the correct permission and the non-authoritative versions
 * are just writeable files.
 */
DEFINE_TEST(test_write_disk_hardlink)
{
#if defined(__HAIKU__)
	skipping("archive_write_disk_hardlink; hardlinks are not supported on bfs");
#else
	static const char data[]="abcdefghijklmnopqrstuvwxyz";
	struct archive *ad;
	struct archive_entry *ae;
#ifdef HAVE_LINKAT
	int can_symlink;
#endif
	int r;

	/* Force the umask to something predictable. */
	assertUmask(UMASK);

	/* Write entries to disk. */
	assert((ad = archive_write_disk_new()) != NULL);

	/*
	 * First, use a tar-like approach; a regular file, then
	 * a separate "hardlink" entry.
	 */

	/* Regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link1a");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, sizeof(data));
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(sizeof(data),
	    archive_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	/* Link.  Size of zero means this doesn't carry data. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link1b");
	archive_entry_set_mode(ae, S_IFREG | 0642);
	archive_entry_set_size(ae, 0);
	archive_entry_copy_hardlink(ae, "link1a");
	assertEqualIntA(ad, 0, r = archive_write_header(ad, ae));
	if (r >= ARCHIVE_WARN) {
		assertEqualInt(ARCHIVE_WARN,
		    archive_write_data(ad, data, sizeof(data)));
		assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	}
	archive_entry_free(ae);

	/*
	 * Repeat tar approach test, but use unset to mark the
	 * hardlink as having no data.
	 */

	/* Regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link2a");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, sizeof(data));
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(sizeof(data),
	    archive_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	/* Link.  Unset size means this doesn't carry data. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link2b");
	archive_entry_set_mode(ae, S_IFREG | 0642);
	archive_entry_unset_size(ae);
	archive_entry_copy_hardlink(ae, "link2a");
	assertEqualIntA(ad, 0, r = archive_write_header(ad, ae));
	if (r >= ARCHIVE_WARN) {
		assertEqualInt(ARCHIVE_WARN,
		    archive_write_data(ad, data, sizeof(data)));
		assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	}
	archive_entry_free(ae);

	/*
	 * Second, try an old-cpio-like approach; a regular file, then
	 * another identical one (which has been marked hardlink).
	 */

	/* Regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link3a");
	archive_entry_set_mode(ae, S_IFREG | 0600);
	archive_entry_set_size(ae, sizeof(data));
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(sizeof(data), archive_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	/* Link. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link3b");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, sizeof(data));
	archive_entry_copy_hardlink(ae, "link3a");
	assertEqualIntA(ad, 0, r = archive_write_header(ad, ae));
	if (r > ARCHIVE_WARN) {
		assertEqualInt(sizeof(data),
		    archive_write_data(ad, data, sizeof(data)));
		assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	}
	archive_entry_free(ae);

	/*
	 * Third, try a new-cpio-like approach, where the initial
	 * regular file is empty and the hardlink has the data.
	 */

	/* Regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link4a");
	archive_entry_set_mode(ae, S_IFREG | 0600);
	archive_entry_set_size(ae, 0);
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(ARCHIVE_WARN, archive_write_data(ad, data, 1));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	/* Link. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link4b");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, sizeof(data));
	archive_entry_copy_hardlink(ae, "link4a");
	assertEqualIntA(ad, 0, r = archive_write_header(ad, ae));
	if (r > ARCHIVE_FAILED) {
		assertEqualInt(sizeof(data),
		    archive_write_data(ad, data, sizeof(data)));
		assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	}
	archive_entry_free(ae);

#ifdef HAVE_LINKAT
	/* Finally, try creating a hard link to a dangling symlink */
	can_symlink = canSymlink();
	if (can_symlink) {
		/* Symbolic link: link5a -> foo */
		assert((ae = archive_entry_new()) != NULL);
		archive_entry_copy_pathname(ae, "link5a");
		archive_entry_set_mode(ae, AE_IFLNK | 0642);
		archive_entry_unset_size(ae);
		archive_entry_copy_symlink(ae, "foo");
		assertEqualIntA(ad, 0, r = archive_write_header(ad, ae));
		if (r >= ARCHIVE_WARN) {
			assertEqualInt(ARCHIVE_WARN,
			    archive_write_data(ad, data, sizeof(data)));
			assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
		}
		archive_entry_free(ae);


		/* Link.  Size of zero means this doesn't carry data. */
		assert((ae = archive_entry_new()) != NULL);
		archive_entry_copy_pathname(ae, "link5b");
		archive_entry_set_mode(ae, S_IFREG | 0642);
		archive_entry_set_size(ae, 0);
		archive_entry_copy_hardlink(ae, "link5a");
		assertEqualIntA(ad, 0, r = archive_write_header(ad, ae));
		if (r >= ARCHIVE_WARN) {
			assertEqualInt(ARCHIVE_WARN,
			    archive_write_data(ad, data, sizeof(data)));
			assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
		}
		archive_entry_free(ae);
	}
#endif
	assertEqualInt(0, archive_write_free(ad));

	/* Test the entries on disk. */

	/* Test #1 */
	/* If the hardlink was successfully created and the archive
	 * doesn't carry data for it, we consider it to be
	 * non-authoritative for meta data as well.  This is consistent
	 * with GNU tar and BSD pax.  */
	assertIsReg("link1a", 0755 & ~UMASK);
	assertFileSize("link1a", sizeof(data));
	assertFileNLinks("link1a", 2);
	assertIsHardlink("link1a", "link1b");

	/* Test #2: Should produce identical results to test #1 */
	/* Note that marking a hardlink with size = 0 is treated the
	 * same as having an unset size.  This is partly for backwards
	 * compatibility (we used to not have unset tracking, so
	 * relied on size == 0) and partly to match the model used by
	 * common file formats that store a size of zero for
	 * hardlinks. */
	assertIsReg("link2a", 0755 & ~UMASK);
	assertFileSize("link2a", sizeof(data));
	assertFileNLinks("link2a", 2);
	assertIsHardlink("link2a", "link2b");

	/* Test #3 */
	assertIsReg("link3a", 0755 & ~UMASK);
	assertFileSize("link3a", sizeof(data));
	assertFileNLinks("link3a", 2);
	assertIsHardlink("link3a", "link3b");

	/* Test #4 */
	assertIsReg("link4a", 0755 & ~UMASK);
	assertFileNLinks("link4a", 2);
	assertFileSize("link4a", sizeof(data));
	assertIsHardlink("link4a", "link4b");

#ifdef HAVE_LINKAT
	if (can_symlink) {
		/* Test #5 */
		assertIsSymlink("link5a", "foo", 0);
		assertFileNLinks("link5a", 2);
		assertIsHardlink("link5a", "link5b");
	}
#endif
#endif
}

DEFINE_TEST(test_write_disk_hardlink_skip_file)
{
#if defined(__HAIKU__) || (defined(_WIN32) && !defined(__CYGWIN__)) || \
    (!HAVE_LINK && !HAVE_LINKAT)
	skipping("Hardlinks are not supported on this platform");
#else
	static const char original[] = "BASELINE-ORIGINAL\n";
	static const char replacement[] = "NEW-DATA";
	static const int flags[] = {
		ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM,
		ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
		    ARCHIVE_EXTRACT_NO_OVERWRITE | ARCHIVE_EXTRACT_SAFE_WRITES
	};
	struct archive *ad;
	struct archive_entry *ae;
	struct stat before, after;
	char target[48], alias[48];
	size_t i;
	int r;

	assertUmask(022);

	/* Hardlink with data targeting skip_file must fail without truncating it. */
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
		snprintf(target, sizeof(target), "skip_target_%u", (unsigned)i);
		snprintf(alias, sizeof(alias), "skip_alias_%u", (unsigned)i);
		assertMakeFile(target, 0600, original);
		assertEqualInt(0, stat(target, &before));

		assert((ad = archive_write_disk_new()) != NULL);
		assertEqualIntA(ad, ARCHIVE_OK,
		    archive_write_disk_set_options(ad, flags[i]));
		assertEqualIntA(ad, ARCHIVE_OK,
		    archive_write_disk_set_skip_file(ad,
		    before.st_dev, before.st_ino));

		assert((ae = archive_entry_new()) != NULL);
		archive_entry_copy_pathname(ae, alias);
		archive_entry_copy_hardlink(ae, target);
		archive_entry_set_mode(ae, AE_IFREG | 0644);
		archive_entry_set_mtime(ae, before.st_mtime - 3600, 0);
		archive_entry_set_size(ae, sizeof(replacement) - 1);

		r = archive_write_header(ad, ae);
		failure("flags=0x%x", flags[i]);
		assertEqualIntA(ad, ARCHIVE_FAILED, r);

		assertFileSize(target, sizeof(original) - 1);
		assertTextFileContents(original, target);

		if (r >= ARCHIVE_WARN)
			(void)archive_write_data(ad, replacement,
			    sizeof(replacement) - 1);
		assertEqualIntA(ad, ARCHIVE_OK,
		    archive_write_finish_entry(ad));
		assertEqualIntA(ad, ARCHIVE_OK,
		    archive_write_close(ad));
		assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
		archive_entry_free(ae);

		assertEqualInt(0, stat(target, &after));
		assertEqualInt(before.st_dev, after.st_dev);
		assertEqualInt(before.st_ino, after.st_ino);
		assertEqualInt(before.st_mtime, after.st_mtime);
		assertIsReg(target, 0600);
		assertFileSize(target, sizeof(original) - 1);
		assertTextFileContents(original, target);
	}

	/* Hardlink without data (size == 0) to skip_file. */
	assertMakeFile("zero_target", 0600, original);
	assertEqualInt(0, stat("zero_target", &before));
	assert((ad = archive_write_disk_new()) != NULL);
	assertEqualIntA(ad, ARCHIVE_OK,
	    archive_write_disk_set_options(ad,
	    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM));
	assertEqualIntA(ad, ARCHIVE_OK,
	    archive_write_disk_set_skip_file(ad,
	    before.st_dev, before.st_ino));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "zero_alias");
	archive_entry_copy_hardlink(ae, "zero_target");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_mtime(ae, before.st_mtime - 3600, 0);
	archive_entry_set_size(ae, 0);
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_header(ad, ae));
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_close(ad));
	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
	archive_entry_free(ae);
	assertIsHardlink("zero_target", "zero_alias");
	assertEqualInt(0, stat("zero_target", &after));
	assertEqualInt(before.st_dev, after.st_dev);
	assertEqualInt(before.st_ino, after.st_ino);
	assertEqualInt(before.st_mtime, after.st_mtime);
	assertIsReg("zero_target", 0600);
	assertFileSize("zero_target", sizeof(original) - 1);
	assertTextFileContents(original, "zero_target");

	/* Normal hardlink with data (no skip_file set). */
	assertMakeFile("control_target", 0600, original);
	assert((ad = archive_write_disk_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "control_alias");
	archive_entry_copy_hardlink(ae, "control_target");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, sizeof(replacement) - 1);
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_header(ad, ae));
	assertEqualInt(sizeof(replacement) - 1,
	    archive_write_data(ad, replacement, sizeof(replacement) - 1));
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_close(ad));
	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
	archive_entry_free(ae);
	assertIsHardlink("control_target", "control_alias");
	assertFileSize("control_target", sizeof(replacement) - 1);
	assertTextFileContents(replacement, "control_target");
#endif
}
