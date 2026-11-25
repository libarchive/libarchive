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

#define POISON_BLOCKS 64
#define POISON_SIZE (256 * 1024)

/*
 * In a solid RAR5 archive, a later entry can declare a larger
 * dictionary than the earlier entries.  The reader then grows the
 * window buffer; the newly added part must not expose uninitialized
 * heap memory to back-references in the compressed stream.
 */
DEFINE_TEST(test_read_format_rar5_solid_window_grow)
{
	const char *reffile = "test_read_format_rar5_solid_window_grow.rar";
	static const unsigned char zeros[4] = { 0, 0, 0, 0 };
	struct archive_entry *ae;
	struct archive *a;
	unsigned char buf[16];
	ssize_t r;
	void *junk[POISON_BLOCKS];
	int i;

	/*
	 * Leave recognizable garbage in freed heap memory so that, if
	 * the grown window were not cleared, the leaked bytes would
	 * show up in the output.  (Whether the allocator reuses these
	 * blocks is platform-dependent; under MSan/Valgrind the bug is
	 * detected regardless.)
	 */
	for (i = 0; i < POISON_BLOCKS; i++) {
		junk[i] = malloc(POISON_SIZE);
		if (junk[i] != NULL)
			memset(junk[i], 0x53, POISON_SIZE);
	}
	for (i = 0; i < POISON_BLOCKS; i++)
		free(junk[i]);

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("seed.bin", archive_entry_pathname(ae));
	r = archive_read_data(a, buf, sizeof(buf));
	assertEqualInt(1, r);
	assertEqualInt(0x41, buf[0]);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("leak.bin", archive_entry_pathname(ae));
	memset(buf, 0xff, sizeof(buf));
	r = archive_read_data(a, buf, sizeof(buf));
	assertEqualInt(4, r);
	assertEqualMem(buf, zeros, sizeof(zeros));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
