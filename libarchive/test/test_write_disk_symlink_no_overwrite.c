/*-
 * Copyright (c) 2026 Koi
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
 * A symlink entry must not replace an existing object when
 * ARCHIVE_EXTRACT_NO_OVERWRITE is set.
 */
DEFINE_TEST(test_write_disk_symlink_no_overwrite)
{
	static const char data[] = "original";
	struct archive *ad;
	struct archive_entry *ae;

	/* An existing regular file must be left alone. */
	assertMakeFile("file", 0644, data);

	assert((ad = archive_write_disk_new()) != NULL);
	assertEqualIntA(ad, ARCHIVE_OK,
	    archive_write_disk_set_options(ad, ARCHIVE_EXTRACT_NO_OVERWRITE));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFLNK | 0755);
	archive_entry_copy_symlink(ae, "target");
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_header(ad, ae));
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	archive_entry_free(ae);
	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));

	assertIsReg("file", -1);
	assertFileContents(data, sizeof(data) - 1, "file");

	/* An existing directory must be left alone. */
	assertMakeDir("dir", 0755);

	assert((ad = archive_write_disk_new()) != NULL);
	assertEqualIntA(ad, ARCHIVE_OK,
	    archive_write_disk_set_options(ad, ARCHIVE_EXTRACT_NO_OVERWRITE));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_mode(ae, AE_IFLNK | 0755);
	archive_entry_copy_symlink(ae, "target");
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_header(ad, ae));
	assertEqualIntA(ad, ARCHIVE_OK, archive_write_finish_entry(ad));
	archive_entry_free(ae);
	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));

	assertIsDir("dir", -1);
}
