/*-
 * Copyright (c) 2003-2008 Tim Kientzle
 * Copyright (c) 2009,2010 Michihiro NAKAJIMA
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
read_image(const unsigned char *buff, size_t used, const char *pathname,
    int expected)
{
	struct archive *a;
	struct archive_entry *ae;
	char data[8];
	int r;

	if (!assert((a = archive_read_new()) != NULL))
		return;
	/* Do not let another format recognize a rejected ISO as an empty file. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_iso9660(a));
	r = archive_read_open_memory(a, buff, used);
	assertEqualIntA(a, expected, r);
	if (r == ARCHIVE_OK && expected == ARCHIVE_OK) {
		if (!assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_next_header(a, &ae)))
			goto done;
		assertEqualString(".", archive_entry_pathname(ae));
		if (!assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_next_header(a, &ae)))
			goto done;
		assertEqualString(pathname, archive_entry_pathname(ae));
		assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
		assertEqualInt(8, archive_entry_size(ae));
		assertEqualIntA(a, 8, archive_read_data(a, data, sizeof(data)));
		assertEqualMem("contents", data, sizeof(data));
		assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	}
done:
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_reserved_fields(const char *options, const char *pathname, int sector,
    int version, const int *offsets, size_t count)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buff, *descriptor;
	unsigned char saved;
	size_t buffsize = 512 * 1024;
	size_t used = 0;
	size_t i;
	const int invalid_offsets[] = {1, 881, 156 + 25};

	if (!assert((buff = malloc(buffsize)) != NULL))
		return;
	if (!assert((a = archive_write_new()) != NULL)) {
		free(buff);
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_iso9660(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, options));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_write_free(a);
		free(buff);
		return;
	}
	archive_entry_set_pathname(ae, "file.txt");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 8);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 8, archive_write_data(a, "contents", 8));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	if (!assert(used >= (size_t)(sector + 1) * 2048)) {
		free(buff);
		return;
	}
	descriptor = buff + sector * 2048;
	assertEqualInt(sector == 16 ? 1 : 2, descriptor[0]);
	assertEqualInt(version, descriptor[6]);
	read_image(buff, used, pathname, ARCHIVE_OK);

	/* Change one reserved field at a time, leaving the file data intact. */
	for (i = 0; i < count; ++i) {
		saved = descriptor[offsets[i]];
		descriptor[offsets[i]] = 'X';
		read_image(buff, used, pathname, ARCHIVE_OK);
		descriptor[offsets[i]] = saved;
	}

	/* The signature, file structure version and root flags are required. */
	for (i = 0; i < sizeof(invalid_offsets) / sizeof(invalid_offsets[0]);
	    ++i) {
		saved = descriptor[invalid_offsets[i]];
		descriptor[invalid_offsets[i]] = 0;
		read_image(buff, used, pathname, ARCHIVE_FATAL);
		descriptor[invalid_offsets[i]] = saved;
	}
	free(buff);
}

DEFINE_TEST(test_read_format_iso9660_reserved)
{
	const int pvd_offsets[] = {72, 88, 1395};
	const int svd_offsets[] = {72, 882, 1395};
	const int evd_offsets[] = {72, 88, 882, 1395};

	test_reserved_fields("!rockridge,!joliet", "FILE.TXT", 16, 1,
	    pvd_offsets, sizeof(pvd_offsets) / sizeof(pvd_offsets[0]));
	test_reserved_fields("!rockridge", "file.txt", 17, 1,
	    svd_offsets, sizeof(svd_offsets) / sizeof(svd_offsets[0]));
	test_reserved_fields("!rockridge,!joliet,iso-level=4", "file.txt", 17, 2,
	    evd_offsets, sizeof(evd_offsets) / sizeof(evd_offsets[0]));
}
