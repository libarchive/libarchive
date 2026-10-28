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

static void
test_malformed1(void)
{
	const char *refname = "test_read_format_zip_malformed1.zip";
	struct archive *a;
	struct archive_entry *ae;
	char *p;
	size_t s;

	extract_reference_file(refname);

	/* Verify with seeking reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Verify with streaming reader. */
	p = slurpfile(&s, "%s", refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory(a, p, s, 31));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(p);
}

/*
 * A local file header can declare an arbitrarily large compressed size.  The
 * skip path must not attempt to lseek past the end of the input: doing so is
 * legal on POSIX and, with 1 GB steps, turns "list" into a multi-billion-
 * iteration loop (issue #2142).  Skipping such an entry has to fail promptly.
 */
static const unsigned char oversized_zip[] = {
	0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x08, 0x00, 0x08, 0x00, 0xa6, 0xae,
	0x3e, 0x57, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xf5, 0x00,
	0x00, 0x00, 0x06, 0x00, 0x20, 0x00, 0x63, 0x73, 0x76, 0x2e, 0x70, 0x79,
	0x01, 0x00, 0x10, 0x00, 0xf5, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x66, 0x4c,
	0x13, 0x1f, 0x66, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0xf5, 0x01, 0x00,
	0x00, 0x04, 0x14, 0x00, 0x00, 0x00, 0x4d, 0x8f, 0xcd, 0x0a, 0xc3, 0x20,
	0x10, 0x84, 0xef, 0x79, 0x8a, 0xc5, 0x00, 0x14, 0x52, 0xcf, 0xa5, 0x90,
	0x63, 0x0b, 0x3d, 0x50, 0xff, 0x08, 0x00, 0xa6, 0x00, 0x00, 0x00, 0x08,
	0x41, 0x00, 0x32, 0x80, 0xe4, 0x54, 0x65, 0xda, 0x5f, 0x45, 0x50, 0xc6,
	0xe3, 0x3c, 0x1c, 0xd4, 0x29, 0x8e, 0x9f, 0xa7, 0x32, 0xb0, 0xdf, 0xc8,
	0xeb, 0xc8, 0x00, 0x00, 0x06, 0x00, 0x20, 0x00, 0x63, 0x73, 0x76, 0x2e,
	0x70, 0x79, 0x55, 0x54, 0x0d, 0x00, 0x07, 0x99, 0x6e, 0x18, 0x65, 0x8a,
	0x1f, 0x26, 0x66, 0x4c, 0x13, 0x1f, 0x66, 0x75, 0x78, 0x0b, 0x00, 0x01,
	0x04, 0xf5, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x50, 0x4b, 0x05, 0x06,
	0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0xb3, 0x00, 0x00, 0x00,
	0x00, 0x02, 0x2d, 0x00, 0x00, 0x00, 0x4e, 0x35, 0x7f, 0x45, 0xbf, 0x8c,
	0x4b, 0xd6, 0xb9, 0x48,
};

/*
 * Streaming reader (in-memory input, no seek): reading the first header must
 * succeed and skipping its oversized body must terminate with an error rather
 * than looping forever.
 */
static void
test_oversized_entry_skip(void)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    read_open_memory(a, oversized_zip, sizeof(oversized_zip), 31));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("csv.py", archive_entry_pathname(ae));
	assert(archive_read_data_skip(a) < ARCHIVE_OK);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Seeking reader (regular file): same archive, same requirement.
 */
static void
test_oversized_entry_skip_seekable(void)
{
	struct archive *a;
	struct archive_entry *ae;
	const char *fname = "test_read_format_zip_oversized.zip";
	FILE *f;

	/* Materialize the malformed archive so the seeking path is used. */
	f = fopen(fname, "wb");
	assert(f != NULL);
	assertEqualInt(sizeof(oversized_zip),
	    fwrite(oversized_zip, 1, sizeof(oversized_zip), f));
	assertEqualInt(0, fclose(f));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, fname, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("csv.py", archive_entry_pathname(ae));
	/*
	 * Must fail (truncated/malformed), not loop forever.  The skipped
	 * distance is clamped to what the file actually holds (128 bytes),
	 * which distinguishes a bounded skip from an lseek past EOF.
	 */
	assert(archive_read_data_skip(a) < ARCHIVE_OK);
	assert(strstr(archive_error_string(a),
	    "Truncated input file") != NULL);
	assert(strstr(archive_error_string(a),
	    "only 128 available") != NULL);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Control: a well-formed archive whose entry has a large (but honest) declared
 * size must still list and extract correctly, so the fix above does not reject
 * legitimate archives.
 */
static void
test_legitimate_large_entry(void)
{
	struct archive *a;
	struct archive *aw;
	struct archive_entry *ae;
	char *buf;
	size_t used;
	const size_t payload = 100000;
	char *data;

	/* Build a valid zip with a large (honest) entry. */
	assert((aw = archive_write_new()) != NULL);
	assertEqualIntA(aw, ARCHIVE_OK, archive_write_set_format_zip(aw));
	buf = malloc(payload + 1024);
	assert(buf != NULL);
	assertEqualIntA(aw, ARCHIVE_OK,
	    archive_write_open_memory(aw, buf, payload + 1024, &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "big.txt");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, (la_int64_t)payload);
	archive_entry_set_perm(ae, 0644);
	assertEqualIntA(aw, ARCHIVE_OK, archive_write_header(aw, ae));
	data = malloc(payload);
	assert(data != NULL);
	memset(data, 'A', payload);
	assertEqualInt((int)payload, (int)archive_write_data(aw, data, payload));
	free(data);
	archive_entry_free(ae);
	assertEqualIntA(aw, ARCHIVE_OK, archive_write_close(aw));
	archive_write_free(aw);

	/* Must list and extract correctly. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory(a, buf, used, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("big.txt", archive_entry_pathname(ae));
	{
		char *out = malloc(payload);
		assert(out != NULL);
		assertEqualInt((int)payload,
		    (int)archive_read_data(a, out, payload));
		assertEqualInt('A', out[0]);
		assertEqualInt('A', out[payload - 1]);
		free(out);
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(buf);
}

DEFINE_TEST(test_read_format_zip_malformed)
{
	test_malformed1();
	test_oversized_entry_skip();
	test_oversized_entry_skip_seekable();
	test_legitimate_large_entry();
}