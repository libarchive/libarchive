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

DEFINE_TEST(test_archive_ckd_add_i64)
{
	int64_t r;

	assert(archive_ckd_add_i64(&r, 0, 0) == 0);
	assertEqualInt(0, r);

	assert(archive_ckd_add_i64(&r, 1, 2) == 0);
	assertEqualInt(3, r);

	/* INT64_MAX and INT64_MIN are representable. */
	assert(archive_ckd_add_i64(&r, INT64_MAX, 0) == 0);
	assertEqualInt(INT64_MAX, r);
	assert(archive_ckd_add_i64(&r, INT64_MIN, 0) == 0);
	assertEqualInt(INT64_MIN, r);

	/* INT64_MIN + INT64_MAX == -1 is exactly representable. */
	assert(archive_ckd_add_i64(&r, INT64_MIN, INT64_MAX) == 0);
	assertEqualInt(-1, r);

	/* Overflow in either direction must be reported, not wrapped. */
	assert(archive_ckd_add_i64(&r, INT64_MAX, 1) != 0);
	assert(archive_ckd_add_i64(&r, INT64_MIN, -1) != 0);
	assert(archive_ckd_add_i64(&r, INT64_MAX, INT64_MAX) != 0);
	assert(archive_ckd_add_i64(&r, INT64_MIN, INT64_MIN) != 0);
}

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

DEFINE_TEST(test_archive_ckd_add_u64)
{
	uint64_t r;

	assert(archive_ckd_add_u64(&r, 0, 0) == 0);
	assertEqualInt(0, r);

	assert(archive_ckd_add_u64(&r, 1, 2) == 0);
	assertEqualInt(3, r);

	/* Largest representable sum. */
	assert(archive_ckd_add_u64(&r, UINT64_MAX, 0) == 0);
	assertEqualInt(UINT64_MAX, r);

	/* Overflow must be reported, not wrapped. */
	assert(archive_ckd_add_u64(&r, UINT64_MAX, 1) != 0);
	assert(archive_ckd_add_u64(&r, UINT64_MAX, UINT64_MAX) != 0);
}

DEFINE_TEST(test_archive_ckd_mul_i64)
{
	int64_t r;

	assert(archive_ckd_mul_i64(&r, 0, 0) == 0);
	assertEqualInt(0, r);
	assert(archive_ckd_mul_i64(&r, 0, INT64_MAX) == 0);
	assertEqualInt(0, r);
	assert(archive_ckd_mul_i64(&r, INT64_MIN, 0) == 0);
	assertEqualInt(0, r);

	assert(archive_ckd_mul_i64(&r, 3, 4) == 0);
	assertEqualInt(12, r);
	assert(archive_ckd_mul_i64(&r, -3, 4) == 0);
	assertEqualInt(-12, r);
	assert(archive_ckd_mul_i64(&r, -3, -4) == 0);
	assertEqualInt(12, r);

	/* Identity products stay within the range. */
	assert(archive_ckd_mul_i64(&r, INT64_MAX, 1) == 0);
	assertEqualInt(INT64_MAX, r);
	assert(archive_ckd_mul_i64(&r, INT64_MIN, 1) == 0);
	assertEqualInt(INT64_MIN, r);

	/*
	 * 3037000499 is the largest integer whose square still fits
	 * in int64_t.
	 */
	assert(archive_ckd_mul_i64(&r, 3037000499, 3037000499) == 0);
	assert(archive_ckd_mul_i64(&r, 3037000500, 3037000500) != 0);

	/* Overflow must be reported, not wrapped. */
	assert(archive_ckd_mul_i64(&r, INT64_MAX, 2) != 0);
	assert(archive_ckd_mul_i64(&r, INT64_MIN, 2) != 0);
	/* INT64_MIN * -1 needs one more than INT64_MAX. */
	assert(archive_ckd_mul_i64(&r, INT64_MIN, -1) != 0);
	assert(archive_ckd_mul_i64(&r, INT64_MIN, INT64_MIN) != 0);
}

DEFINE_TEST(test_archive_ckd_mul_size)
{
	size_t r;

	assert(archive_ckd_mul_size(&r, 0, 0) == 0);
	assertEqualInt(0, r);
	assert(archive_ckd_mul_size(&r, 0, SIZE_MAX) == 0);
	assertEqualInt(0, r);

	assert(archive_ckd_mul_size(&r, 3, 4) == 0);
	assertEqualInt(12, r);

	/* Identity product stays within the range. */
	assert(archive_ckd_mul_size(&r, SIZE_MAX, 1) == 0);
	assertEqualInt(SIZE_MAX, r);

	/* SIZE_MAX is odd, so SIZE_MAX / 2 * 2 still fits. */
	assert(archive_ckd_mul_size(&r, SIZE_MAX / 2, 2) == 0);
	assertEqualInt(SIZE_MAX - 1, r);

	/* Overflow must be reported, not wrapped. */
	assert(archive_ckd_mul_size(&r, SIZE_MAX, 2) != 0);
	assert(archive_ckd_mul_size(&r, SIZE_MAX / 2 + 1, 2) != 0);
	assert(archive_ckd_mul_size(&r, SIZE_MAX, SIZE_MAX) != 0);
}

DEFINE_TEST(test_archive_ckd_mul_u64)
{
	uint64_t r;

	assert(archive_ckd_mul_u64(&r, 0, 0) == 0);
	assertEqualInt(0, r);
	assert(archive_ckd_mul_u64(&r, 0, UINT64_MAX) == 0);
	assertEqualInt(0, r);

	assert(archive_ckd_mul_u64(&r, 3, 4) == 0);
	assertEqualInt(12, r);

	/* Identity product stays within the range. */
	assert(archive_ckd_mul_u64(&r, UINT64_MAX, 1) == 0);
	assertEqualInt(UINT64_MAX, r);

	/* UINT64_MAX is odd, so UINT64_MAX / 2 * 2 still fits. */
	assert(archive_ckd_mul_u64(&r, UINT64_MAX / 2, 2) == 0);
	assertEqualInt(UINT64_MAX - 1, r);

	/* Overflow must be reported, not wrapped. */
	assert(archive_ckd_mul_u64(&r, UINT64_MAX, 2) != 0);
	assert(archive_ckd_mul_u64(&r, UINT64_MAX / 2 + 1, 2) != 0);
	assert(archive_ckd_mul_u64(&r, UINT64_MAX, UINT64_MAX) != 0);
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

	/* Overflow above the signed range must be reported too. */
	assert(archive_ckd_sub_i64(&r, INT64_MAX, -1) != 0);
	assert(archive_ckd_sub_i64(&r, 0, INT64_MIN) != 0);
}
