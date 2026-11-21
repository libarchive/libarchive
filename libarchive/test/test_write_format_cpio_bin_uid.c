/*-
 * Copyright (c) 2026 Andrew Stellman
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
 * The binary cpio formats store the uid and gid in 16 bits.
 * An entry whose uid or gid doesn't fit must be refused, not
 * written with the value wrapped (65536 would become 0, root).
 */
static void
test_format(int (*set_format)(struct archive *))
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[4096];
	size_t used;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, set_format(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	/* uid 65536 is refused. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "uid");
	archive_entry_set_mode(ae, AE_IFREG | 04755);
	archive_entry_set_size(ae, 0);
	archive_entry_set_uid(ae, 65536);
	archive_entry_set_gid(ae, 1);
	assertEqualIntA(a, ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* gid 65536 is refused. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "gid");
	archive_entry_set_mode(ae, AE_IFREG | 02755);
	archive_entry_set_size(ae, 0);
	archive_entry_set_uid(ae, 1);
	archive_entry_set_gid(ae, 65536);
	assertEqualIntA(a, ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* 65535 still fits. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "max");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 0);
	archive_entry_set_uid(ae, 65535);
	archive_entry_set_gid(ae, 65535);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	/* Only the entry that fits was written. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("max", archive_entry_pathname(ae));
	assertEqualInt(65535, archive_entry_uid(ae));
	assertEqualInt(65535, archive_entry_gid(ae));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_write_format_cpio_bin_uid)
{
	test_format(archive_write_set_format_cpio_bin);
	test_format(archive_write_set_format_cpio_pwb);
}
