/*-
 * Copyright (c) 2026 Tim Kientzle
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
 * Issue #1118: Closing an archive before an external decompression
 * program has finished writing its output must not be reported as
 * an error.
 *
 * When the reader stops early (e.g., "bsdtar -q" after finding the
 * requested entry), the child's stdout pipe is closed while the child
 * is still writing.  The child should then be killed by SIGPIPE, which
 * archive_read_support_filter_program.c treats as a normal exit.
 *
 * But bsdtar, bsdcpio, and this test harness all ignore SIGPIPE (so
 * that we see EPIPE instead of dying when a child exits early), and an
 * ignored signal stays ignored across exec().  If that disposition
 * leaks into the child, the child instead sees EPIPE, typically prints
 * "Broken pipe" and exits with a non-zero status, which the reader
 * reports as an error.
 *
 * To reproduce, we build a gzip-compressed tar holding 8 MiB of zeros
 * (a few KiB compressed), read only the first header through the
 * external "gzip -d" program, and close.  The decompressed output is
 * far larger than any pipe buffer, so gzip is guaranteed to still be
 * writing when we close its stdout.
 */

#define	BIG_FILE_SIZE	(8 * 1024 * 1024)

DEFINE_TEST(test_read_filter_program_early_close)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	skipping("SIGPIPE semantics are not applicable on Windows");
#else
	static char zeros[65536];
	struct archive_entry *ae;
	struct archive *a;
	size_t buffsize = 1024 * 1024;
	char *buff;
	size_t used;
	int i, r;

	if (!canGzip()) {
		skipping("Can't run gzip program on this platform");
		return;
	}

	/* Build a gzip-compressed tar archive in memory. */
	assert((buff = malloc(buffsize)) != NULL);
	if (buff == NULL)
		return;
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	/* Without zlib, this falls back to the gzip program (ARCHIVE_WARN). */
	r = archive_write_add_filter_gzip(a);
	assert(r == ARCHIVE_OK || r == ARCHIVE_WARN);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "big");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0644);
	archive_entry_set_size(ae, BIG_FILE_SIZE);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	for (i = 0; i < BIG_FILE_SIZE / (int)sizeof(zeros); i++)
		assertEqualIntA(a, sizeof(zeros),
		    archive_write_data(a, zeros, sizeof(zeros)));
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "after");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0644);
	archive_entry_set_size(ae, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Read only the first header via external "gzip -d", then close. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_program(a, "gzip -d"));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("big", archive_entry_pathname(ae));
	assertEqualInt(archive_filter_code(a, 0), ARCHIVE_FILTER_PROGRAM);

	/* The child is still writing; closing must not report an error. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
#endif
}
