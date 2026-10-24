/*-
 * Copyright (c) 2026 krishna28238-arch
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the disclaimer below.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the disclaimer in the
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

#include "archive_integer.h"

#ifdef SSIZE_MIN
#define NEG_SSIZE SSIZE_MIN
#else
#define NEG_SSIZE ((ssize_t)(~(((size_t)-1) >> 1)))
#endif

DEFINE_TEST(test_archive_ckd_add_size)
{
	size_t r;

	assert(archive_ckd_add_size(&r, 0, 0) == 0);
	assertEqualInt(0, r);

	assert(archive_ckd_add_size(&r, 1, 2) == 0);
	assertEqualInt(3, r);

	/* Largest representable sum. */
	assert(archive_ckd_add_size(&r, SIZE_MAX, 0) == 0);
	assertEqualInt(SIZE_MAX, r);

	/* Overflow must be reported, not wrapped. */
	assert(archive_ckd_add_size(&r, SIZE_MAX, 1) != 0);
	assert(archive_ckd_add_size(&r, SIZE_MAX, SIZE_MAX) != 0);
}

DEFINE_TEST(test_archive_ckd_sub_size)
{
	size_t r;

	assert(archive_ckd_sub_size(&r, 0, 0) == 0);
	assertEqualInt(0, r);

	assert(archive_ckd_sub_size(&r, 10, 3) == 0);
	assertEqualInt(7, r);

	/* Exact zero is representable. */
	assert(archive_ckd_sub_size(&r, 5, 5) == 0);
	assertEqualInt(0, r);

	assert(archive_ckd_sub_size(&r, SIZE_MAX, SIZE_MAX) == 0);
	assertEqualInt(0, r);

	/* Underflow must be reported, not wrapped to a huge value. */
	assert(archive_ckd_sub_size(&r, 0, 1) != 0);
	assert(archive_ckd_sub_size(&r, 4, 5) != 0);
	assert(archive_ckd_sub_size(&r, SIZE_MAX - 1, SIZE_MAX) != 0);
}

DEFINE_TEST(test_archive_ckd_sub_u64)
{
	uint64_t r;

	assert(archive_ckd_sub_u64(&r, 10, 3) == 0);
	assertEqualInt(7, r);

	assert(archive_ckd_sub_u64(&r, UINT64_MAX, UINT64_MAX) == 0);
	assertEqualInt(0, r);

	assert(archive_ckd_sub_u64(&r, 0, 1) != 0);
	assert(archive_ckd_sub_u64(&r, 0x1234, 0x1235) != 0);
}

DEFINE_TEST(test_archive_ckd_sub_i64)
{
	int64_t r;

	assert(archive_ckd_sub_i64(&r, 10, 3) == 0);
	assertEqualInt(7, r);

	assert(archive_ckd_sub_i64(&r, INT64_MIN, INT64_MIN) == 0);
	assertEqualInt(0, r);

	/* Underflow below the signed range must be reported. */
	assert(archive_ckd_sub_i64(&r, INT64_MIN, 1) != 0);
	assert(archive_ckd_sub_i64(&r, INT64_MIN, INT64_MAX) != 0);
	/* -1 - INT64_MAX == INT64_MIN is exactly representable. */
	assert(archive_ckd_sub_i64(&r, -1, INT64_MAX) == 0);
	assertEqualInt(INT64_MIN, r);
}

DEFINE_TEST(test_archive_saturating_cast_u32)
{
	/* Negative read results are turned into 0. */
	assertEqualInt(0, archive_saturating_cast_u32(-1));
	assertEqualInt(0, archive_saturating_cast_u32(NEG_SSIZE));

	assertEqualInt(0, archive_saturating_cast_u32(0));
	assertEqualInt(1, archive_saturating_cast_u32(1));
	assertEqualInt(65535, archive_saturating_cast_u32(65535));

	/* Values that fit pass through unchanged. */
	assertEqualInt(UINT32_MAX, archive_saturating_cast_u32(UINT32_MAX));

	/* Larger values saturate at UINT32_MAX. */
	assertEqualInt(UINT32_MAX,
	    archive_saturating_cast_u32((uint64_t)UINT32_MAX + 1));
	assertEqualInt(UINT32_MAX, archive_saturating_cast_u32(INT64_MAX));
}

DEFINE_TEST(test_archive_saturating_cast_i32)
{
	/* Negative read results are turned into 0. */
	assertEqualInt(0, archive_saturating_cast_i32(-1));
	assertEqualInt(0, archive_saturating_cast_i32(NEG_SSIZE));

	assertEqualInt(0, archive_saturating_cast_i32(0));
	assertEqualInt(1, archive_saturating_cast_i32(1));
	assertEqualInt(4096, archive_saturating_cast_i32(4096));

	/* Values that fit pass through unchanged. */
	assertEqualInt(INT32_MAX, archive_saturating_cast_i32(INT32_MAX));

	/* Larger values saturate at INT32_MAX. */
	assertEqualInt(INT32_MAX,
	    archive_saturating_cast_i32((int64_t)INT32_MAX + 1));
	assertEqualInt(INT32_MAX, archive_saturating_cast_i32(INT64_MAX));
}
