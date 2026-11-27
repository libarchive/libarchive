/*-
 * Copyright (c) 2026 mizuki
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

/*
 * The EX_HASH and EX_HTIME parsers read their fixed-size items without
 * checking them against the size the record declares for itself, unlike
 * the EX_REDIR and EX_UOWNER parsers and the handling of unknown record
 * types in process_head_file_extra().  In the first archive below, an
 * EX_HASH record declares a 1-byte payload (the hash type only), so its
 * 32-byte digest is read from the EX_HTIME record that follows, which is
 * silently swallowed and the entry loses its timestamps while the header
 * still returns ARCHIVE_OK.  In the second archive, an EX_HTIME record
 * declares only its flags byte and takes its mtime value from the bytes
 * after it.  Both records must be rejected with ARCHIVE_FATAL instead.
 */
DEFINE_TEST(test_read_format_rar5_extra_record_toosmall)
{
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_file(
	    "test_read_format_rar5_extra_record_toosmall.rar");
	a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_rar5(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a,
	    "test_read_format_rar5_extra_record_toosmall.rar", 10240));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	extract_reference_file(
	    "test_read_format_rar5_extra_record_toosmall_htime.rar");
	a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_rar5(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a,
	    "test_read_format_rar5_extra_record_toosmall_htime.rar", 10240));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
