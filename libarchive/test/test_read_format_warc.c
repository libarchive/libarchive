/*-
 * Copyright (c) 2014 Sebastian Freundt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

DEFINE_TEST(test_read_format_warc)
{
	char buff[256U];
	const char reffile[] = "test_read_format_warc.warc";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));

	/* First Entry */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("sometest.txt", archive_entry_pathname(ae));
	assertEqualInt(1402399833, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(65, archive_entry_size(ae));
	assertEqualInt(65, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "This is a sample text file for libarchive's WARC reader/writer.\n\n", 65);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	/* Second Entry */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("moretest.txt", archive_entry_pathname(ae));
	assertEqualInt(1402399884, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(76, archive_entry_size(ae));
	assertA(76 == archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "The beauty is that WARC remains ASCII only iff all contents are ASCII only.\n", 76);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	/* Test EOF */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(2, archive_file_count(a));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_WARC, archive_format(a));

	/* Verify closing and resource freeing */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_warc_incomplete)
{
	const char reffile[] = "test_read_format_warc_incomplete.warc";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));

	/* Entry cannot be parsed */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));

	/* Verify closing and resource freeing */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_warc_skip_after_extract)
{
#define WARC_APPEND(s) do { \
	memcpy(p, s, sizeof(s) - 1); \
	p += sizeof(s) - 1; \
} while (0)
	const size_t body_size = 200000;
	char *warc;
	char *p;
	char second[6];
	size_t used;
	int fd;
	struct archive_entry *ae;
	struct archive *a;

	warc = malloc(body_size + 1024);
	assert(warc != NULL);
	p = warc;

	WARC_APPEND("WARC/1.0\r\n"
	    "WARC-Type: resource\r\n"
	    "WARC-Date: 2014-06-10T10:10:10Z\r\n"
	    "WARC-Target-URI: file://first.txt\r\n"
	    "Content-Length: 200000\r\n"
	    "\r\n");
	memset(p, 'A', body_size);
	p += body_size;
	WARC_APPEND("\r\n\r\n"
	    "WARC/1.0\r\n"
	    "WARC-Type: resource\r\n"
	    "WARC-Date: 2014-06-10T10:10:11Z\r\n"
	    "WARC-Target-URI: file://second.txt\r\n"
	    "Content-Length: 6\r\n"
	    "\r\n"
	    "SECOND"
	    "\r\n\r\n");
	used = (size_t)(p - warc);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory2(a, warc, used, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("first.txt", archive_entry_pathname(ae));

	fd = open("warc.out", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(fd >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, fd));
	assertEqualInt(0, close(fd));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("second.txt", archive_entry_pathname(ae));
	assertEqualInt(6, archive_entry_size(ae));
	assertEqualInt(6, archive_read_data(a, second, sizeof(second)));
	assertEqualMem(second, "SECOND", 6);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(warc);
#undef WARC_APPEND
}

DEFINE_TEST(test_read_format_warc_truncated_body)
{
	static const char warc[] =
	    "WARC/1.0\r\n"
	    "WARC-Type: resource\r\n"
	    "WARC-Date: 2014-06-10T10:10:10Z\r\n"
	    "WARC-Target-URI: file://test.txt\r\n"
	    "Content-Length: 10\r\n"
	    "\r\n"
	    "ABC";
	char body[3];
	struct archive_entry *ae;
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, warc, sizeof(warc) - 1U));

	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualInt(3,
	    archive_read_data(a, body, sizeof(body)));
	assertEqualMem(body, "ABC", sizeof(body));
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_data(a, body, sizeof(body)));
	assertEqualString("Truncated WARC file data",
	    archive_error_string(a));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_warc_uri_nul)
{
	const char reffile[] = "test_read_format_warc_uri_nul.warc";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));

	/* Entry cannot be parsed */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));

	/* Verify closing and resource freeing */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_warc_future_date)
{
	static const char warc[] =
	    "WARC/1.0\r\n"
	    "WARC-Type: resource\r\n"
	    "WARC-Date: 2039-01-01T00:00:00Z\r\n"
	    "WARC-Target-URI: file://test.txt\r\n"
	    "Content-Length: 0\r\n"
	    "\r\n"
	    "\r\n\r\n";
	struct archive_entry *ae;
	struct archive *a;

	if (sizeof(time_t) < 8) {
		skipping("This test requires a 64-bit time_t");
		return;
	}

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, warc, sizeof(warc) - 1U));

	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualInt(2177452800LL, archive_entry_mtime(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_warc_header_across_blocks)
{
	static const char hdr_fmt[] =
	    "WARC/1.0\r\nWARC-Type: resource\r\n"
	    "WARC-Target-URI: file://%s\r\n"
	    "WARC-Date: 2020-01-01T00:00:00Z\r\n"
	    "Content-Length: %d\r\n\r\n";
	const size_t hdr2_offset = 20 * 10240 - 50;
	struct archive_entry *ae;
	struct archive *a;
	char hdr[256];
	char *buff;
	size_t used;
	int body1, n;

	/* Choose the first body size so that the second header lands
	 * where we want it. */
	body1 = (int)hdr2_offset;
	do {
		body1--;
		n = snprintf(hdr, sizeof(hdr), hdr_fmt, "a.txt", body1);
	} while ((size_t)n + body1 + 4 > hdr2_offset);
	assertEqualInt(hdr2_offset, n + body1 + 4);

	assert((buff = malloc(hdr2_offset + 512)) != NULL);
	memcpy(buff, hdr, n);
	memset(buff + n, 'a', body1);
	memcpy(buff + n + body1, "\r\n\r\n", 4);
	used = hdr2_offset;
	n = snprintf(hdr, sizeof(hdr), hdr_fmt, "b.txt", 3);
	memcpy(buff + used, hdr, n);
	used += n;
	memcpy(buff + used, "bbb\r\n\r\n", 7);
	used += 7;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    read_open_memory_minimal(a, buff, used, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("a.txt", archive_entry_pathname(ae));
	assertEqualInt(body1, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("b.txt", archive_entry_pathname(ae));
	assertEqualInt(3, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(buff);
}

static char *
make_long_header_record(size_t hdr_len, int terminate, size_t *used)
{
	static const char start[] =
	    "WARC/1.0\r\nWARC-Type: resource\r\n"
	    "WARC-Target-URI: file://long.txt\r\n"
	    "WARC-Date: 2020-01-01T00:00:00Z\r\n"
	    "Content-Length: 3\r\n"
	    "X-Pad: ";
	char *buff;
	size_t pad;

	/* The fixed part and a 4-byte terminator must fit in hdr_len. */
	if (!assert(hdr_len >= sizeof(start) - 1 + 4))
		return (NULL);
	pad = hdr_len - (sizeof(start) - 1) - 4;
	buff = malloc(hdr_len + 16);
	if (buff == NULL)
		return (NULL);
	memcpy(buff, start, sizeof(start) - 1);
	memset(buff + sizeof(start) - 1, 'p', pad);
	*used = sizeof(start) - 1 + pad;
	if (terminate) {
		memcpy(buff + *used, "\r\n\r\nhi\n\r\n\r\n", 11);
		*used += 11;
	} else {
		memset(buff + *used, 'p', 4);
		*used += 4;
	}
	return (buff);
}

static void
read_long_header(const char *buff, size_t used, int minimal,
    int expect_ok, const char *desc)
{
	struct archive_entry *ae;
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_warc(a));
	/* failure() only labels the next assertion. */
	failure("%s", desc);
	if (minimal)
		assertEqualIntA(a, ARCHIVE_OK,
		    read_open_memory_minimal(a, buff, used, 10240));
	else
		assertEqualIntA(a, ARCHIVE_OK,
		    read_open_memory(a, buff, used, used));
	if (expect_ok) {
		failure("%s", desc);
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_next_header(a, &ae));
		failure("%s", desc);
		assertEqualString("long.txt", archive_entry_pathname(ae));
		failure("%s", desc);
		assertEqualInt(3, archive_entry_size(ae));
		failure("%s", desc);
		assertEqualIntA(a, ARCHIVE_EOF,
		    archive_read_next_header(a, &ae));
	} else {
		failure("%s", desc);
		assertEqualIntA(a, ARCHIVE_FATAL,
		    archive_read_next_header(a, &ae));
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_warc_header_max_len)
{
	static const struct {
		size_t len;
		int terminate, ok;
	} cases[] = {
		{ 1024 * 1024, 1, 1 },
		{ 1024 * 1024 + 1, 1, 0 },
		{ 200, 0, 0 },
	};
	char desc[128];
	char *buff;
	size_t i, used;
	int minimal;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		buff = make_long_header_record(cases[i].len,
		    cases[i].terminate, &used);
		if (!assert(buff != NULL))
			continue;
		for (minimal = 0; minimal <= 1; minimal++) {
			snprintf(desc, sizeof(desc),
			    "case %d: header length %d, %s, %s read",
			    (int)i, (int)cases[i].len,
			    cases[i].terminate ? "terminated" : "truncated",
			    minimal ? "10240-byte" : "whole-buffer");
			read_long_header(buff, used, minimal, cases[i].ok,
			    desc);
		}
		free(buff);
	}
}
