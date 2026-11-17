/*-
 * Copyright (c) 2026 Corey Daley
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
 * The gzip trailer holds the CRC32 and length (mod 2^32) of the
 * uncompressed data. Damage that inflate doesn't notice, such as a
 * changed byte in a stored block, is only caught there.
 */

#define DATA_SIZE (256 * 1024)

/* A gzip stream of DATA_SIZE bytes in stored blocks (compression level
 * 0), so a changed payload byte still inflates. */
static size_t
make_gzip(char *buff, size_t buff_size, const char *data)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t used;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_gzip(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "compression-level", "0"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buff_size, &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, DATA_SIZE);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(DATA_SIZE, archive_write_data(a, data, DATA_SIZE));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	return (used);
}

/* Read all of `buff`; returns the last archive_read_data() result and
 * the number of bytes read. */
static int
read_all(const char *buff, size_t size, size_t *total, char *errbuf)
{
	struct archive *a;
	struct archive_entry *ae;
	char block[8192];
	la_ssize_t r;
	int ret;

	*total = 0;
	errbuf[0] = '\0';
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_gzip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, size));
	ret = archive_read_next_header(a, &ae);
	if (ret == ARCHIVE_OK) {
		while ((r = archive_read_data(a, block, sizeof(block))) > 0)
			*total += r;
		ret = (int)r;
	}
	if (ret != ARCHIVE_OK && archive_error_string(a) != NULL)
		strncpy(errbuf, archive_error_string(a), 255);
	archive_read_free(a);
	return (ret);
}

DEFINE_TEST(test_read_filter_gzip_trailer)
{
	char *data, *buff, *twice;
	char errbuf[256];
	size_t used, total, i;
	const size_t buff_size = DATA_SIZE + 64 * 1024;

	if (archive_zlib_version() == NULL) {
		skipping("zlib not available");
		return;
	}
	assert((data = malloc(DATA_SIZE)) != NULL);
	assert((buff = malloc(buff_size)) != NULL);
	assert((twice = malloc(2 * buff_size)) != NULL);
	for (i = 0; i < DATA_SIZE; i++)
		data[i] = (char)(i * 7 + (i >> 9));
	used = make_gzip(buff, buff_size, data);

	/* An intact stream reads back whole. */
	assertEqualInt(ARCHIVE_OK, read_all(buff, used, &total, errbuf));
	assertEqualInt(DATA_SIZE, total);

	/* So do two members one after the other: each has its own trailer. */
	memcpy(twice, buff, used);
	memcpy(twice + used, buff, used);
	assertEqualInt(ARCHIVE_OK, read_all(twice, 2 * used, &total, errbuf));
	assertEqualInt(2 * DATA_SIZE, total);

	/* A changed byte in the middle of the data inflates without complaint;
	 * only the CRC catches it. */
	buff[used / 2] ^= 0x01;
	assertEqualInt(ARCHIVE_FATAL, read_all(buff, used, &total, errbuf));
	assertEqualString("Gzip: CRC32 error", errbuf);
	buff[used / 2] ^= 0x01;

	/* A wrong CRC in the trailer. */
	buff[used - 8] ^= 0x01;
	assertEqualInt(ARCHIVE_FATAL, read_all(buff, used, &total, errbuf));
	assertEqualString("Gzip: CRC32 error", errbuf);
	buff[used - 8] ^= 0x01;

	/* A wrong length in the trailer. */
	buff[used - 4] ^= 0x01;
	assertEqualInt(ARCHIVE_FATAL, read_all(buff, used, &total, errbuf));
	assertEqualString("Gzip: Uncompressed size error", errbuf);
	buff[used - 4] ^= 0x01;

	assertEqualInt(ARCHIVE_OK, read_all(buff, used, &total, errbuf));

	free(twice);
	free(buff);
	free(data);
}
