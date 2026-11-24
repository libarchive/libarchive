/*-
 * Copyright (c) 2026 Sri Vedantam
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

DEFINE_TEST(test_read_format_zip_7855_short)
{
	struct archive *a;
	struct archive_entry *entry;
	unsigned char *zip;
	size_t datasize;

	/* Exactly sized local headers with 0 to 4 bytes of Ux data. */
	for (datasize = 0; datasize <= 4; ++datasize) {
		zip = calloc(1, 35 + datasize);
		assert(zip != NULL);
		zip[0] = 'P';
		zip[1] = 'K';
		zip[2] = 3;
		zip[3] = 4;
		zip[4] = 20;
		zip[26] = 1;
		zip[28] = (unsigned char)(4 + datasize);
		zip[30] = 'a';
		zip[31] = 0x55;
		zip[32] = 0x78;
		zip[33] = (unsigned char)datasize;
		if (datasize >= 1)
			zip[35] = 0x34;
		if (datasize >= 2)
			zip[36] = 0x12;
		if (datasize >= 3)
			zip[37] = 0x78;
		if (datasize >= 4)
			zip[38] = 0x56;

		assert((a = archive_read_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_zip_streamable(a));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_open_memory(a, zip, 35 + datasize));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_next_header(a, &entry));
		assertEqualString("a", archive_entry_pathname(entry));
		assertEqualInt(datasize >= 2 ? 0x1234 : 0,
		    archive_entry_uid(entry));
		assertEqualInt(datasize >= 4 ? 0x5678 : 0,
		    archive_entry_gid(entry));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		free(zip);
	}
}
