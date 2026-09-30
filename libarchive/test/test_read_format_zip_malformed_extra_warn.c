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
 * Regression tests for https://github.com/libarchive/libarchive/issues/1162:
 * a malformed ZIP extra field must surface as ARCHIVE_WARN (or, at worst,
 * be silently skipped), never escalate to ARCHIVE_FATAL. Before this fix,
 * process_extra() returning anything but ARCHIVE_OK made both of its
 * callers return ARCHIVE_FATAL, which kills the entire archive_read
 * object - not just the one malformed entry. That's a real liability for
 * any tool (a malware/content scanner, for instance) that depends on
 * libarchive being able to open and enumerate an archive's entries: a
 * single self-contradictory extra field anywhere in the file could be
 * used to make the whole archive unreadable.
 *
 * Both archives below carry one entry ("test.txt") with an extra field
 * that claims 200 bytes of data while only 4 are actually present - the
 * same "Extra data overflow" shape reported in issue #1162 - but placed
 * in a different header to exercise each of process_extra()'s two call
 * sites:
 *   - malformed_local:   only the Local Header carries it, so it's
 *     re-parsed by zip_read_local_file_header() while returning this
 *     specific entry: archive_read_next_header() must return
 *     ARCHIVE_WARN (not ARCHIVE_FATAL), and the entry must still be
 *     fully readable.
 *   - malformed_central: only the Central Directory carries it, so it's
 *     hit during slurp_central_directory()'s upfront prescan of every
 *     entry, before any single entry has been returned to the caller:
 *     the prescan must not abort the whole archive just because one
 *     entry's Central Directory extra field is malformed.
 */

/* Malformed only in the Local Header. */
static const unsigned char archive_malformed_extra_local[] = {
/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time */
	0x21, 0x00,             /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x08, 0x00,             /* extra field length = 8 (malformed - see below) */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
	0x99, 0x99,             /* extra field ID = 0x9999 (arbitrary/unrecognized) */
	0xc8, 0x00,             /* extra field size = 200 (claimed - see below) */
	0xaa, 0xaa, 0xaa, 0xaa, /* only 4 bytes actually present: malformed */
/* --- file data --- */
	0x68, 0x69, 0x0a,       /* "hi\n" */
/* --- central directory header --- */
	0x50, 0x4b, 0x01, 0x02, /* central directory header signature */
	0x14, 0x00,             /* version made by: 2.0, MS-DOS */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time */
	0x21, 0x00,             /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x00, 0x00,             /* extra field length = 0 (no extra field here) */
	0x00, 0x00,             /* file comment length */
	0x00, 0x00,             /* disk number start */
	0x00, 0x00,             /* internal file attributes */
	0x00, 0x00, 0xa4, 0x81, /* external file attributes: -rw-r--r-- */
	0x00, 0x00, 0x00, 0x00, /* relative offset of local header = 0 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* --- end of central directory record --- */
	0x50, 0x4b, 0x05, 0x06, /* end of central directory signature */
	0x00, 0x00,             /* number of this disk */
	0x00, 0x00,             /* disk with start of central directory */
	0x01, 0x00,             /* central directory entries on this disk */
	0x01, 0x00,             /* total central directory entries */
	0x36, 0x00, 0x00, 0x00, /* size of central directory = 54 */
	0x31, 0x00, 0x00, 0x00, /* offset of central directory = 49 */
	0x00, 0x00,             /* .ZIP file comment length */
};

DEFINE_TEST(test_read_format_zip_malformed_extra_warn_local)
{
	struct archive *a;
	struct archive_entry *ae;
	const void *buff;
	size_t size;
	int64_t offset;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_malformed_extra_local,
		sizeof(archive_malformed_extra_local)));

	/* A malformed extra field is a warning, not a fatal error: the
	 * entry is still returned, with everything the Local Header could
	 * establish outside of that one field. */
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	assertEqualString("test.txt", archive_entry_pathname(ae));
	assertEqualInt(3, archive_entry_size(ae));

	/* The archive_read object must still be usable: entry data can
	 * still be extracted, and the object can still be closed cleanly -
	 * neither would be true if this had escalated to ARCHIVE_FATAL. */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_data_block(a, &buff, &size, &offset));
	assertEqualInt(3, (int)size);
	assertEqualMem(buff, "hi\n", 3);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* Malformed only in the Central Directory. */
static const unsigned char archive_malformed_extra_central[] = {
/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time */
	0x21, 0x00,             /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x00, 0x00,             /* extra field length = 0 (no extra field here) */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* --- file data --- */
	0x68, 0x69, 0x0a,       /* "hi\n" */
/* --- central directory header --- */
	0x50, 0x4b, 0x01, 0x02, /* central directory header signature */
	0x14, 0x00,             /* version made by: 2.0, MS-DOS */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time */
	0x21, 0x00,             /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x08, 0x00,             /* extra field length = 8 (malformed - see below) */
	0x00, 0x00,             /* file comment length */
	0x00, 0x00,             /* disk number start */
	0x00, 0x00,             /* internal file attributes */
	0x00, 0x00, 0xa4, 0x81, /* external file attributes: -rw-r--r-- */
	0x00, 0x00, 0x00, 0x00, /* relative offset of local header = 0 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
	0x99, 0x99,             /* extra field ID = 0x9999 (arbitrary/unrecognized) */
	0xc8, 0x00,             /* extra field size = 200 (claimed - see below) */
	0xaa, 0xaa, 0xaa, 0xaa, /* only 4 bytes actually present: malformed */
/* --- end of central directory record --- */
	0x50, 0x4b, 0x05, 0x06, /* end of central directory signature */
	0x00, 0x00,             /* number of this disk */
	0x00, 0x00,             /* disk with start of central directory */
	0x01, 0x00,             /* central directory entries on this disk */
	0x01, 0x00,             /* total central directory entries */
	0x3e, 0x00, 0x00, 0x00, /* size of central directory = 62 */
	0x29, 0x00, 0x00, 0x00, /* offset of central directory = 41 */
	0x00, 0x00,             /* .ZIP file comment length */
};

DEFINE_TEST(test_read_format_zip_malformed_extra_warn_central)
{
	struct archive *a;
	struct archive_entry *ae;
	const void *buff;
	size_t size;
	int64_t offset;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_malformed_extra_central,
		sizeof(archive_malformed_extra_central)));

	/* Before the fix, this malformed field was hit during the
	 * up-front Central Directory prescan, before any entry had been
	 * returned at all: archive_read_next_header() for the very FIRST
	 * entry would return ARCHIVE_FATAL here, with no way to recover.
	 * It must now open the entry normally. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("test.txt", archive_entry_pathname(ae));
	assertEqualInt(3, archive_entry_size(ae));

	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_data_block(a, &buff, &size, &offset));
	assertEqualInt(3, (int)size);
	assertEqualMem(buff, "hi\n", 3);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
