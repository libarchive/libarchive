/*-
 * Copyright (c) 2026 mundur
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
 * Restoring metadata must not follow a symlink that an earlier entry in the
 * same archive created.
 *
 * The reference archive holds:
 *
 *   dest/link          symlink -> ../victim.txt
 *   dest/hl            hardlink -> dest/link, declared as a regular file
 *                      with mode 04777 and a size of 8
 *
 * A tar hardlink entry (typeflag '1') may declare a non-zero size, which makes
 * the reader present it as a regular file that also carries a body.  The object
 * that create_filesystem_object() actually creates, though, is whatever
 * linkat() produces, and linkat() does not follow its source: hardlinking an
 * existing symlink yields a second name for the symlink inode.  The entry
 * therefore claims AE_IFREG while the name on disk is a symlink.
 *
 * If the metadata phase trusts the declared type it selects chmod(), which
 * *does* follow symlinks, and the mode carried by the archive is applied to the
 * symlink's target -- here a file outside the extraction directory, and even
 * though ARCHIVE_EXTRACT_SECURE_SYMLINKS is in effect.
 *
 * This is the regression: the file outside the destination must keep its
 * original mode.
 */

DEFINE_TEST(test_write_disk_hardlink_to_symlink)
{
#if !defined(HAVE_LINKAT) || (defined(_WIN32) && !defined(__CYGWIN__))
	skipping("Platform cannot hardlink to a symlink");
#else
	const char *name = "test_write_disk_hardlink_to_symlink.tar";
	struct archive *a, *ad;
	struct archive_entry *ae;
	struct stat st;
	mode_t victim_mode;
	int r;

	if (!canSymlink()) {
		skipping("Platform cannot create symlinks");
		return;
	}

	assertMakeDir("dest", 0755);
	assertMakeFile("victim.txt", 0600, "victim");
	assertEqualInt(0, stat("victim.txt", &st));
	victim_mode = st.st_mode & 07777;
	assertEqualInt(0600, (int)victim_mode);

	extract_reference_file(name);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, name, 10240));

	assert((ad = archive_write_disk_new()) != NULL);
	/* The same flags bsdtar uses by default. */
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_disk_set_options(ad,
	    ARCHIVE_EXTRACT_SECURE_SYMLINKS | ARCHIVE_EXTRACT_SECURE_NODOTDOT));

	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
		const void *buff;
		size_t size;
		int64_t offset;

		r = archive_write_header(ad, ae);
		if (r < ARCHIVE_WARN)
			break;
		for (;;) {
			r = archive_read_data_block(a, &buff, &size, &offset);
			if (r == ARCHIVE_EOF)
				break;
			if (r < ARCHIVE_OK)
				break;
			/* The hardlink entry has no open fd, so this warns. */
			assert(archive_write_data_block(ad, buff, size,
			    offset) >= ARCHIVE_WARN);
		}
		assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	}
	assert(r == ARCHIVE_EOF || r == ARCHIVE_WARN);

	assertEqualIntA(ad, ARCHIVE_OK, archive_write_close(ad));
	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* "dest/hl" is a second name for the symlink inode. */
	assertEqualInt(0, lstat("dest/hl", &st));
	assertEqualInt(1, S_ISLNK(st.st_mode) ? 1 : 0);

	/* The file outside the destination must be untouched. */
	assertEqualInt(0, stat("victim.txt", &st));
	failure("Metadata of a hardlink to a symlink leaked outside the "
	    "destination: victim.txt mode is %04o, expected %04o",
	    (unsigned)(st.st_mode & 07777), (unsigned)victim_mode);
	assertEqualInt((int)victim_mode, (int)(st.st_mode & 07777));
#endif
}
