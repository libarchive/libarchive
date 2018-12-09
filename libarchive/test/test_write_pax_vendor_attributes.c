/*-
 * Copyright (c) 2016 IBM Corporation
 * Copyright (c) 2003-2007 Tim Kientzle
 *
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
 *
 * This test case's code has been derived from test_entry.c
 */
#include "test.h"

static char buff[1000000];

DEFINE_TEST(test_write_pax_vendor_attributes)
{
	struct archive *ar, *aw;
	struct archive_entry *ae;
	const char *name;
	const void *val;
	size_t size;
	const char *string;
	char *p;
	size_t used;


	assert((aw = archive_write_new()) != NULL);
	assertEqualIntA(aw, ARCHIVE_OK, archive_write_set_format_pax(aw));
	assertEqualIntA(aw, ARCHIVE_OK,
		archive_write_open_memory(aw, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);
	// Build entry
	string = "aaaa";
	archive_entry_vendor_add_entry(ae, "CUSTOM.key1", string, strlen(string) + 1);
	string = "bbbb";
	archive_entry_vendor_add_entry(ae, "CUSTOM.key2", string, strlen(string) + 1);
	string = "cccc";
	archive_entry_vendor_add_entry(ae, "CUSTOM.key3", string, strlen(string) + 1);
	string = "invalid";
	archive_entry_vendor_add_entry(ae, "invalid.key3", string, strlen(string) + 1);
	// Add required components for tar archive
	p = strdup("file");
	archive_entry_copy_pathname(ae, p);
	strcpy(p, "XXXX");
	free(p);
	assertEqualString("file", archive_entry_pathname(ae));
	archive_entry_set_size(ae, 8);
	archive_entry_set_mode(ae, S_IFREG | 0755);
	// Add entry to archive
	assertEqualIntA(aw, ARCHIVE_OK, archive_write_header(aw, ae));
	archive_entry_free(ae);
	// Close the archive
	assertEqualIntA(aw, ARCHIVE_OK, archive_write_close(aw));
	assertEqualInt(ARCHIVE_OK, archive_write_free(aw));


	// Read the archive
	assert((ar = archive_read_new()) != NULL);
	assertEqualIntA(ar, ARCHIVE_OK, archive_read_support_format_all(ar));
	assertEqualIntA(ar, ARCHIVE_OK, archive_read_support_filter_all(ar));
	assertEqualIntA(ar, ARCHIVE_OK, archive_read_open_memory(ar, buff, used));
	assertEqualIntA(ar, ARCHIVE_OK, archive_read_next_header(ar, &ae));
	// Verify the entry
	assertEqualInt(3, archive_entry_vendor_count(ae));
	assertEqualInt(3, archive_entry_vendor_reset(ae));
	// Verify entry 1
	assertEqualInt(0, archive_entry_vendor_next(ae, &name, &val, &size));
	assertEqualString(name, "CUSTOM.key1");
	string = "aaaa";
	assertEqualInt((int)size, strlen(string) + 1);
	assertEqualString(val, string);
	// Verify entry 2
	assertEqualInt(0, archive_entry_vendor_next(ae, &name, &val, &size));
	assertEqualString(name, "CUSTOM.key2");
	string = "bbbb";
	assertEqualInt((int)size, strlen(string) + 1);
	assertEqualString(val, string);
	// Verify entry 3
	assertEqualInt(0, archive_entry_vendor_next(ae, &name, &val, &size));
	assertEqualString(name, "CUSTOM.key3");
	string = "cccc";
	assertEqualInt((int)size, strlen(string) + 1);
	assertEqualString(val, string);

	/* Close the archive. */
	assertEqualIntA(ar, ARCHIVE_OK, archive_read_close(ar));
	assertEqualInt(ARCHIVE_OK, archive_read_free(ar));
}
