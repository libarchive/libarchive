/*-
 * Copyright (c) 2026 François Degros
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
 * A minimal ZIP archive with a single empty-ish entry ("ntfs.txt") whose
 * local and central extra data both carry an 0x000A "NTFS" extra field
 * (reserved(4) + Tag(1)/Size(24): Mtime, Atime, Ctime as 8-byte Windows
 * FILETIME values), and no 0x5455 "UT" field. This exercises reading
 * that field directly, rather than through libarchive's own writer,
 * since libarchive doesn't write 0x000A.
 *
 * The three FILETIME values encode, respectively:
 *   Mtime: 2024-03-15T10:20:30.123456000Z
 *   Atime: 2024-03-16T11:21:31.234567000Z
 *   Ctime: 2024-03-14T09:19:29.345678000Z (Windows creation time, which
 *          is why it must map to archive_entry_birthtime(), not ctime)
 */
DEFINE_TEST(test_read_format_zip_ntfs_timestamp)
{
	const char *refname = "test_read_format_zip_ntfs_timestamp.zip";
	struct archive *a;
	struct archive_entry *ae;
	char *p;
	size_t s;

	extract_reference_file(refname);
	p = slurpfile(&s, "%s", refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, p, s, 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("ntfs.txt", archive_entry_pathname(ae));

	assert(archive_entry_mtime_is_set(ae));
	assertEqualInt(1710498030, archive_entry_mtime(ae));
	assertEqualInt(123456000, archive_entry_mtime_nsec(ae));

	assert(archive_entry_atime_is_set(ae));
	assertEqualInt(1710588091, archive_entry_atime(ae));
	assertEqualInt(234567000, archive_entry_atime_nsec(ae));

	/* NTFS "Ctime" is Windows creation time, so it must map to
	 * birthtime, not to POSIX ctime (which ZIP has no source for
	 * here, so it stays unset by this extra field). */
	assert(archive_entry_birthtime_is_set(ae));
	assertEqualInt(1710407969, archive_entry_birthtime(ae));
	assertEqualInt(345678000, archive_entry_birthtime_nsec(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(p);
}
