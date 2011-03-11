/*-
 * Copyright (c) 2003-2011 Tim Kientzle
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
__FBSDID("$FreeBSD$");

#include <locale.h>

/*
 * Pax interchange is supposed to encode filenames into
 * UTF-8.
 */

static void
test_pax_filename_encoding_ru_RU()
{
  	struct archive *a;
  	struct archive_entry *entry;
	char buff[4096];
	size_t used;

	if (NULL == setlocale(LC_ALL, "ru_RU.KOI8-R")) {
		skipping("KOI8-R locale not available on this system.");
		return;
	}

	a = archive_write_new();
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_pax(a));
	assertEqualInt(ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff), &used));

	entry = archive_entry_new2(a);
	archive_entry_set_pathname(entry, "\xD0\xD2\xC9");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Above three characters in KOI8-R should translate to the following
	 * three characters (two bytes each) in UTF-8. */
	assertEqualMem(buff + 512, "15 path=\xD0\xBF\xD1\x80\xD0\xB8\x0A", 15);
}

static void
test_pax_filename_encoding_ja_JP()
{
  	struct archive *a;
  	struct archive_entry *entry;
	char buff[4096];
	wchar_t ws[] = {0x8868, L'.', L't', L'x', L't', 0};
	size_t used;

	if (NULL == setlocale(LC_ALL, "ja_JP.eucJP")) {
		skipping("eucJP locale not available on this system.");
		return;
	}

	a = archive_write_new();
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_pax(a));
	assertEqualInt(ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff), &used));

	entry = archive_entry_new2(a);
	archive_entry_set_pathname(entry, "\xC9\xBD.txt");
	/* Check the Unicode version. */
	assertEqualMem(ws, archive_entry_pathname_w(entry), sizeof(ws));
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Check UTF-8 version. */
	assertEqualMem(buff + 512, "16 path=\xE8\xA1\xA8.txt\x0A", 16);

}

DEFINE_TEST(test_pax_filename_encoding_freebsd)
{
	test_pax_filename_encoding_ru_RU();
	test_pax_filename_encoding_ja_JP();
}
