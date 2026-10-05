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
 * LZMA2 knows where its stream ends, so it stops before the padding that
 * AES adds to the packed data. This must not be an error.
 */
DEFINE_TEST(test_read_format_7zip_encryption_lzma2)
{
	/* Two files in a solid LZMA2 folder, with AES-256 and encrypted
	   headers. Password is "12345678". */
	const char *refname = "test_read_format_7zip_encryption_lzma2.7z";
	static const char line[] =
	    "The quick brown fox jumps over the lazy dog.\n";
	struct archive_entry *ae;
	struct archive *a;
	char buff[1024];
	char expected[20 * sizeof(line)];
	int i;

	for (i = 0; i < 20; i++)
		memcpy(expected + i * (sizeof(line) - 1), line,
		    sizeof(line) - 1);

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_add_passphrase(a, "12345678"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("d/fox.txt", archive_entry_pathname(ae));
	assertEqualInt(900, archive_entry_size(ae));
	assertEqualInt(1, archive_entry_is_data_encrypted(ae));
	assertEqualInt(900, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, expected, 900);
	assertEqualInt(0, archive_read_data(a, buff, sizeof(buff)));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("d/hello.txt", archive_entry_pathname(ae));
	assertEqualInt(6, archive_entry_size(ae));
	assertEqualInt(6, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "hello\n", 6);
	assertEqualInt(0, archive_read_data(a, buff, sizeof(buff)));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
