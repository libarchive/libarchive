/*-
 * Copyright (c) 2026
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

#include <limits.h>

#define __LIBARCHIVE_TEST 1
#include "archive_private.h"

#if ARCHIVE_VERSION_NUMBER < 4000000

DEFINE_TEST(test_archive_file_count)
{
	struct archive a;

	memset(&a, 0, sizeof(a));

	/* Values below INT_MAX are returned as-is. */
	a.file_count = 0;
	assertEqualInt(0, archive_file_count(&a));
	a.file_count = 42;
	assertEqualInt(42, archive_file_count(&a));
	a.file_count = INT_MAX - 1;
	assertEqualInt(INT_MAX - 1, archive_file_count(&a));
	a.file_count = INT_MAX;
	assertEqualInt(INT_MAX, archive_file_count(&a));

	/* Larger counts saturate instead of wrapping negative. */
	a.file_count = (la_int64_t)INT_MAX + 1;
	assertEqualInt(INT_MAX, archive_file_count(&a));
	a.file_count = (la_int64_t)INT_MAX * 2;
	assertEqualInt(INT_MAX, archive_file_count(&a));
}

#else

DEFINE_TEST(test_archive_file_count)
{
	struct archive a;

	memset(&a, 0, sizeof(a));

	/* As of 4.0 the full 64-bit count is exposed. */
	a.file_count = 0;
	assert(archive_file_count(&a) == 0);
	a.file_count = (la_int64_t)INT_MAX + 1;
	assert(archive_file_count(&a) == a.file_count);
}

#endif
