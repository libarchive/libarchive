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
 * A single-entry ZIP where only the Central Directory's copy of the extra
 * data carries a 0x5455 "UT" field (mtime = 1710498031, an odd second
 * that the DOS timestamp cannot represent at all); the Local Header has
 * no extra field, just a DOS timestamp of 10:20:30 (2024-03-15), which
 * rounds down to :30 - one second off from the true mtime.
 *
 * A seekable read prescans the Central Directory first, correctly
 * establishing the precise mtime from its UT field, before walking to
 * the Local Header for each entry. The Local Header's own DOS timestamp
 * must not unconditionally clobber that already-correct value just
 * because the Local Header itself has nothing better to offer.
 */
static const unsigned char archive[] = {
	/* --- Local File Header --- */
	0x50, 0x4b, 0x03, 0x04,             /* Signature "PK\3\4". */
	0x14, 0x00,                         /* Version needed: 2.0. */
	0x00, 0x00,                         /* Flags. */
	0x00, 0x00,                         /* Compression: stored. */
	0x8f, 0x52,                         /* Mod time: 10:20:30. */
	0x6f, 0x58,                         /* Mod date: 2024-03-15. */
	0x7a, 0x7a, 0x6f, 0xed,             /* CRC-32 of "hi\n". */
	0x03, 0x00, 0x00, 0x00,             /* Compressed size: 3. */
	0x03, 0x00, 0x00, 0x00,             /* Uncompressed size: 3. */
	0x08, 0x00,                         /* Filename length: 8. */
	0x00, 0x00,                         /* Extra length: 0 (no
	                                      * extra field at all -
	                                      * the point of this
	                                      * test). */
	't', 'e', 's', 't', '.', 't', 'x', 't',
	'h', 'i', '\n',

	/* --- Central Directory Header --- */
	0x50, 0x4b, 0x01, 0x02,             /* Signature "PK\1\2". */
	0x14, 0x00,                         /* Version made by: 2.0. */
	0x14, 0x00,                         /* Version needed: 2.0. */
	0x00, 0x00,                         /* Flags. */
	0x00, 0x00,                         /* Compression: stored. */
	0x8f, 0x52,                         /* Mod time: 10:20:30. */
	0x6f, 0x58,                         /* Mod date: 2024-03-15. */
	0x7a, 0x7a, 0x6f, 0xed,             /* CRC-32 of "hi\n". */
	0x03, 0x00, 0x00, 0x00,             /* Compressed size: 3. */
	0x03, 0x00, 0x00, 0x00,             /* Uncompressed size: 3. */
	0x08, 0x00,                         /* Filename length: 8. */
	0x09, 0x00,                         /* Extra length: 9 (the
	                                      * "UT" field below). */
	0x00, 0x00,                         /* Comment length: 0. */
	0x00, 0x00,                         /* Disk number start: 0. */
	0x00, 0x00,                         /* Internal attributes. */
	0x00, 0x00, 0xa4, 0x81,             /* External attributes:
	                                      * regular file, 0644. */
	0x00, 0x00, 0x00, 0x00,             /* Local header offset: 0. */
	't', 'e', 's', 't', '.', 't', 'x', 't',
	/* 0x5455 "UT" extended timestamp: mtime only, 1710498031 -
	 * an odd second, which the DOS timestamp above cannot
	 * represent at all (closest even value: 1710498030). */
	'U', 'T',                           /* Tag: 0x5455. */
	0x05, 0x00,                         /* Data size: 5. */
	0x01,                               /* Flags: mtime present. */
	0xef, 0x20, 0xf4, 0x65,             /* mtime: 1710498031. */

	/* --- End Of Central Directory record --- */
	0x50, 0x4b, 0x05, 0x06,             /* Signature "PK\5\6". */
	0x00, 0x00,                         /* Disk number. */
	0x00, 0x00,                         /* Disk with the start of
	                                      * the central directory. */
	0x01, 0x00,                         /* Entries on this disk. */
	0x01, 0x00,                         /* Total entries. */
	0x3f, 0x00, 0x00, 0x00,             /* Central directory size. */
	0x29, 0x00, 0x00, 0x00,             /* Central directory
	                                      * offset. */
	0x00, 0x00,                         /* Comment length. */
};

DEFINE_TEST(test_read_format_zip_mtime_precedence)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("test.txt", archive_entry_pathname(ae));
	assertEqualInt(1710498031, archive_entry_mtime(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Same idea as above, but with an 0x000A "NTFS" field instead of "UT":
 * only the Central Directory's copy of the extra data carries it (Mtime =
 * 2024-03-15T10:20:31.123456000Z, sub-second precision the DOS timestamp
 * can't represent at all); the Local Header has no extra field, just the
 * same coarse DOS timestamp as above.
 *
 * mtime_ns has no DOS-time equivalent to fall back to at all, so this
 * specifically guards against a regression where the Local Header's
 * fallback path resets mtime_ns unconditionally, independently of
 * whether it left mtime itself alone.
 */
static const unsigned char archive_ntfs[] = {
	/* --- Local File Header --- */
	0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x8f, 0x52, 0x6f, 0x58, 0x7a, 0x7a,
	0x6f, 0xed, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00,
	0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x74, 0x65,
	0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, 0x68, 0x69,
	0x0a,

	/* --- Central Directory Header --- */
	0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x8f, 0x52, 0x6f, 0x58,
	0x7a, 0x7a, 0x6f, 0xed, 0x03, 0x00, 0x00, 0x00,
	0x03, 0x00, 0x00, 0x00,
	0x08, 0x00,             /* Filename length: 8. */
	0x24, 0x00,             /* Extra length: 36 (the NTFS field's
	                          * 4-byte ID+size header, plus its
	                          * 32-byte payload). */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xa4, 0x81, 0x00, 0x00, 0x00, 0x00,
	't', 'e', 's', 't', '.', 't', 'x', 't',
	/* 0x000A "NTFS" field: reserved(4), Tag 1, Size 24, then
	 * Mtime/Atime/Ctime as 8-byte Windows FILETIME (all the same
	 * value here; only Mtime matters for this test). */
	0x0a, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x18, 0x00, 0x00, 0xd8, 0x95, 0x68,
	0xc2, 0x76, 0xda, 0x01, 0x00, 0xd8, 0x95, 0x68,
	0xc2, 0x76, 0xda, 0x01, 0x00, 0xd8, 0x95, 0x68,
	0xc2, 0x76, 0xda, 0x01,

	/* --- End Of Central Directory record --- */
	0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x01, 0x00, 0x5a, 0x00, 0x00, 0x00,
	0x29, 0x00, 0x00, 0x00, 0x00, 0x00,
};

DEFINE_TEST(test_read_format_zip_ntfs_mtime_precedence)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_ntfs, sizeof(archive_ntfs)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualInt(1710498031, archive_entry_mtime(ae));
	assertEqualInt(123456000, archive_entry_mtime_nsec(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * An 0x000A "NTFS" field (sub-second precision) immediately followed, in
 * the *same* extra data block, by an 0x5455 "UT" field (1-second
 * precision) confirming the identical whole-second mtime. Since both
 * describe the same instant, the coarser UT field running after NTFS
 * must not discard the sub-second precision NTFS already established -
 * only a field that changes the whole-second value should do that.
 */
static const unsigned char archive_ntfs_then_ut[] = {
	/* --- Local File Header --- */
	0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x8f, 0x52, 0x6f, 0x58, 0x7a, 0x7a,
	0x6f, 0xed, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00,
	0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x74, 0x65,
	0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, 0x68, 0x69,
	0x0a,

	/* --- Central Directory Header --- */
	0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x8f, 0x52, 0x6f, 0x58,
	0x7a, 0x7a, 0x6f, 0xed, 0x03, 0x00, 0x00, 0x00,
	0x03, 0x00, 0x00, 0x00,
	0x08, 0x00,             /* Filename length: 8. */
	0x2d, 0x00,             /* Extra length: 45 (NTFS + UT below). */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xa4, 0x81, 0x00, 0x00, 0x00, 0x00,
	't', 'e', 's', 't', '.', 't', 'x', 't',
	/* 0x000A "NTFS" field, same as the previous test. */
	0x0a, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x18, 0x00, 0x00, 0xd8, 0x95, 0x68,
	0xc2, 0x76, 0xda, 0x01, 0x00, 0xd8, 0x95, 0x68,
	0xc2, 0x76, 0xda, 0x01, 0x00, 0xd8, 0x95, 0x68,
	0xc2, 0x76, 0xda, 0x01,
	/* 0x5455 "UT" field: mtime only, 1710498031 - the exact same
	 * whole second as the NTFS field above. */
	'U', 'T', 0x05, 0x00, 0x01, 0xef, 0x20, 0xf4, 0x65,

	/* --- End Of Central Directory record --- */
	0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x01, 0x00, 0x63, 0x00, 0x00, 0x00,
	0x29, 0x00, 0x00, 0x00, 0x00, 0x00,
};

DEFINE_TEST(test_read_format_zip_ntfs_then_ut_mtime_precedence)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_ntfs_then_ut,
		sizeof(archive_ntfs_then_ut)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualInt(1710498031, archive_entry_mtime(ae));
	assertEqualInt(123456000, archive_entry_mtime_nsec(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
