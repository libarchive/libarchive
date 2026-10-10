/*-
 * Copyright (c) 2026 Zhaoqi Xu
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

#include <locale.h>

/*
 * Round-trip a ZIP_UTF8_NAME entry whose pathname needs more than a
 * single-byte ACP (emoji from #3063). archive_entry_pathname_w() must
 * return the correct wide string so Windows extraction via
 * archive_write_disk_windows does not mojibake the name.
 */
DEFINE_TEST(test_read_format_zip_utf8_pathname_w)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[65536];
	size_t used;
	/* 🐨.txt — same code points as the #3063 repro */
	const char *utf8_name = "\xF0\x9F\x90\xA8.txt";
	const wchar_t *wide_name = L"\U0001f428.txt";
	const char *payload = "hello";

	if (!setCheckedLocale("en_US.UTF-8", "\xC3\xA4", L'\x00E4')) {
		skipping("en_US.UTF-8 locale not available on this system.");
		return;
	}

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	if (archive_write_set_options(a, "hdrcharset=UTF-8") != ARCHIVE_OK) {
		skipping("This system cannot convert character-set for UTF-8.");
		archive_write_free(a);
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((ae = archive_entry_new2(a)) != NULL);
	archive_entry_set_mtime(ae, 1, 0);
	archive_entry_set_pathname(ae, utf8_name);
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, (la_int64_t)strlen(payload));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt((int)strlen(payload),
	    (int)archive_write_data(a, payload, strlen(payload)));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Bit 11 set => ZIP_UTF8_NAME */
	assertEqualInt(0x08, buff[7] & 0x08);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualUTF8String(utf8_name, archive_entry_pathname_utf8(ae));
	assertEqualWString(wide_name, archive_entry_pathname_w(ae));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
