/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
__FBSDID("$FreeBSD: head/lib/libarchive/test/test_pax_filename_encoding.c 201247 2009-12-30 05:59:21Z kientzle $");

#include <locale.h>

/*
 * Pax interchange is supposed to encode filenames into
 * UTF-8.  This verifies that behavior on FreeBSD using
 * the KOI8-R locale.
 */

DEFINE_TEST(test_pax_filename_encoding_freebsd)
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

	entry = archive_entry_new();
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
