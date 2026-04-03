/*-
 * Copyright (c) 2026 Rob Swindell
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
 * Verify extraction of a ZIP archive using method 6 (Implode), the
 * compression method used by PKZIP 1.x before Deflate was introduced.
 *
 * Two flag bits in the local file header control decompression parameters:
 *   bit 1 (0x02): sliding window size — 0 = 4K, 1 = 8K
 *   bit 2 (0x04): Shannon-Fano tree count — 0 = 2 trees, 1 = 3 trees
 *
 * test_read_format_zip_implode:       flags=0x00 (4K window, 2 trees)
 * test_read_format_zip_implode_multi: flags=0x06 (8K window, 3 trees) present
 */

static int
extract_one(struct archive *a, struct archive_entry *ae, uint32_t expected_crc)
{
	la_ssize_t fsize, bytes_read;
	uint8_t *buf;
	int ret = 1;
	uint32_t computed_crc;

	fsize = (la_ssize_t)archive_entry_size(ae);
	buf = malloc(fsize);
	if (buf == NULL)
		return 1;

	bytes_read = archive_read_data(a, buf, fsize);
	if (bytes_read != fsize) {
		assertEqualInt(bytes_read, fsize);
		goto done;
	}

	computed_crc = bitcrc32(0, buf, fsize);
	assertEqualInt(computed_crc, expected_crc);
	ret = 0;
done:
	free(buf);
	return ret;
}

/*
 * Single-entry archive: I.BIN, method 6, flags=0x00 (4K window, 2 trees).
 * Created by Jason Summers as a test case for unimplode6a.
 */
DEFINE_TEST(test_read_format_zip_implode)
{
	const char *refname = "test_read_format_zip_implode.zip";
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("I.BIN", archive_entry_pathname(ae));
	assertEqualInt(2048, archive_entry_size(ae));
	assertEqualIntA(a, 0, extract_one(a, ae, 0x8314ddd9));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_free(a));
}

/*
 * Real-world archive (PKZIP 1.10, 1990) containing entries with both
 * flag variants:
 *   EDIT.ZIP, 4MAP.EXE, PUTMAP.EXE, MAPDEF — flags=0x00 (4K window, 2 trees)
 *   4MAP.DOC                               — flags=0x06 (8K window, 3 trees)
 */
DEFINE_TEST(test_compat_zip_implode)
{
	const char *refname = "test_compat_zip_implode.zip";
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* EDIT.ZIP: flags=0x00, 4K window, 2 trees */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("EDIT.ZIP", archive_entry_pathname(ae));
	assertEqualInt(23303, archive_entry_size(ae));
	assertEqualIntA(a, 0, extract_one(a, ae, 0x79b66e25));

	/* 4MAP.DOC: flags=0x06, 8K window, 3 trees */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("4MAP.DOC", archive_entry_pathname(ae));
	assertEqualInt(16100, archive_entry_size(ae));
	assertEqualIntA(a, 0, extract_one(a, ae, 0xc5ac76b6));

	/* 4MAP.EXE: flags=0x00 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("4MAP.EXE", archive_entry_pathname(ae));
	assertEqualInt(2752, archive_entry_size(ae));
	assertEqualIntA(a, 0, extract_one(a, ae, 0x6c29d4a9));

	/* PUTMAP.EXE: flags=0x00 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("PUTMAP.EXE", archive_entry_pathname(ae));
	assertEqualInt(18551, archive_entry_size(ae));
	assertEqualIntA(a, 0, extract_one(a, ae, 0x3cc85a72));

	/* MAPDEF: flags=0x00 */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("MAPDEF", archive_entry_pathname(ae));
	assertEqualInt(2028, archive_entry_size(ae));
	assertEqualIntA(a, 0, extract_one(a, ae, 0x5fa4c7ac));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_free(a));
}
