/*-
 * Copyright (c) 2026 wlk-worker
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
 * Regression test for a stale format pointer after a failed open.
 *
 * choose_format() used to advance a->format with the loop counter and
 * return ARCHIVE_FATAL without restoring it, leaving a->format pointing
 * one element past the 16-slot formats[] array.  Because formats[] is
 * immediately followed by the "format" member, that address aliases
 * a->format itself, so archive_read_format_capabilities() read a heap
 * value through a bogus descriptor and called it as a function pointer.
 *
 * The 4-byte input is chosen so that no currently registered bidder claims
 * it.  archive_read_support_format_all() installs the stock formats only
 * (plus "empty", which claims zero-length input), and none of them matches
 * this prefix.  The introspection calls below then run against a reader
 * with no format.
 */
DEFINE_TEST(test_archive_read_format_capabilities)
{
	static const unsigned char junk[4] = { 0x00, 0x03, 0x2d, 0xe9 };
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	/* No bidder claims this input; the open is expected to fail. */
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_open_memory(a, junk, sizeof(junk)));

	/* Asking for capabilities after the failure must not crash. */
	assertEqualInt(ARCHIVE_READ_FORMAT_CAPS_NONE,
	    archive_read_format_capabilities(a));
	assertEqualInt(ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED,
	    archive_read_has_encrypted_entries(a));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
