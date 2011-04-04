/*-
 * Copyright (c) 2011 Tim Kientzle
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
__FBSDID("$FreeBSD$");

#include "archive_string.h"

#define EXTENT 32

#define assertStringSizes(strlen, buflen, as) \
	assertEqualInt(strlen, (as).length); \
	assertEqualInt(buflen, (as).buffer_length);

#define assertExactString(strlen, buflen, data, as) \
	do { \
		assertStringSizes(strlen, buflen, as); \
		assertEqualString(data, (as).s); \
	} while (0)

#define assertNonNULLString(strlen, buflen, as) \
	do { \
		assertStringSizes(strlen, buflen, as); \
		assert(NULL != (as).s); \
	} while (0)

#define assertEqualArchiveString(l, r) \

static void
test_archive_string_ensure()
{
	struct archive_string s;

	archive_string_init(&s);
	assertExactString(0, 0, NULL, s);

	/* single-extent allocation */
	assert(NULL != archive_string_ensure(&s, 5));
	assertNonNULLString(0, EXTENT, s);

	/* what happens around extent boundaries? */
	assert(NULL != archive_string_ensure(&s, EXTENT - 1));
	assertNonNULLString(0, EXTENT, s);

	assert(NULL != archive_string_ensure(&s, EXTENT));
	assertNonNULLString(0, EXTENT, s);

	assert(NULL != archive_string_ensure(&s, EXTENT + 1));
	assertNonNULLString(0, 2 * EXTENT, s);
}

static void
test_archive_strcat()
{
	struct archive_string s, *x;

	archive_string_init(&s);
	assertExactString(0, 0, NULL, s);

	/* null target, empty source */
	x = archive_strcat(&s, "");
	assert(NULL != x);
	assertEqualArchiveString(*x, s);
	assertExactString(0, EXTENT, "", s);

	/* empty target, empty source */
	x = archive_strcat(&s, "");
	assert(NULL != x);
	assertEqualArchiveString(*x, s);
	assertExactString(0, EXTENT, "", s);

	/* empty target, non-empty source */
	x = archive_strcat(&s, "fubar");
	assert(NULL != x);
	assertEqualArchiveString(*x, s);
	assertExactString(5, EXTENT, "fubar", s);

	/* non-empty target, non-empty source */
	x = archive_strcat(&s, "baz");
	assert(NULL != x);
	assertEqualArchiveString(*x, s);
	assertExactString(8, EXTENT, "fubarbaz", s);
}

static void
test_archive_strappend_char()
{
	struct archive_string s;

	archive_string_init(&s);
	assertExactString(0, 0, NULL, s);

	/* null target */
	archive_strappend_char(&s, 'X');
	assertExactString(1, EXTENT, "X", s);

	/* non-empty target */
	archive_strappend_char(&s, 'Y');
	assertExactString(2, EXTENT, "XY", s);
}

static void
test_archive_strncat()
{
	struct archive_string s, *x;

	archive_string_init(&s);
	assertExactString(0, 0, NULL, s);

	/* null target, empty source */
	x = archive_strncat(&s, "", 0);
	assert(NULL != x);
	assertEqualArchiveString(*x, s);
	assertExactString(0, EXTENT, "", s);

	/* empty target, empty source */
	x = archive_strncat(&s, "", 0);
	assert(NULL != x);
	assertEqualArchiveString(*x, s);
	assertExactString(0, EXTENT, "", s);

	/* empty target, non-empty source */
	x = archive_strncat(&s, "fubar", 5);
	assert(NULL != x);
	assertEqualArchiveString(*x, s);
	assertExactString(5, EXTENT, "fubar", s);

	/* non-empty target, non-empty source */
	x = archive_strncat(&s, "bazqux", 3);
	assert(NULL != x);
	assertEqualArchiveString(*x, s);
	assertExactString(8, EXTENT, "fubarbaz", s);
}

static void
test_archive_strcpy()
{
	struct archive_string s;

	archive_string_init(&s);
	assertExactString(0, 0, NULL, s);

	/* null target */
	assert(NULL != archive_strcpy(&s, "snafu"));
	assertExactString(5, EXTENT, "snafu", s);

	/* dirty target */
	assert(NULL != archive_strcpy(&s, "foo"));
	assertExactString(3, EXTENT, "foo", s);

	/* dirty target, empty source */
	assert(NULL != archive_strcpy(&s, ""));
	assertExactString(0, EXTENT, "", s);
}

static void
test_archive_string_concat()
{
	struct archive_string s, t, u, v;

	archive_string_init(&s);
	assertExactString(0, 0, NULL, s);
	archive_string_init(&t);
	assertExactString(0, 0, NULL, t);
	archive_string_init(&u);
	assertExactString(0, 0, NULL, u);
	archive_string_init(&v);
	assertExactString(0, 0, NULL, v);

	/* null target, null source */
	archive_string_concat(&t, &s);
	assertExactString(0, 0, NULL, s);
	assertExactString(0, EXTENT, "", t);

	/* null target, empty source */
	assert(NULL != archive_strcpy(&s, ""));
	archive_string_concat(&u, &s);
	assertExactString(0, EXTENT, "", s);
	assertExactString(0, EXTENT, "", u);

	/* null target, non-empty source */
	assert(NULL != archive_strcpy(&s, "foo"));
	archive_string_concat(&v, &s);
	assertExactString(3, EXTENT, "foo", s);
	assertExactString(3, EXTENT, "foo", v);

	/* empty target, empty source */
	assert(NULL != archive_strcpy(&s, ""));
	assert(NULL != archive_strcpy(&t, ""));
	archive_string_concat(&t, &s);
	assertExactString(0, EXTENT, "", s);
	assertExactString(0, EXTENT, "", t);

	/* empty target, non-empty source */
	assert(NULL != archive_strcpy(&s, "snafu"));
	assert(NULL != archive_strcpy(&t, ""));
	archive_string_concat(&t, &s);
	assertExactString(5, EXTENT, "snafu", s);
	assertExactString(5, EXTENT, "snafu", t);
}

static void
test_archive_string_copy()
{
	struct archive_string s, t, u, v;

	archive_string_init(&s);
	assertExactString(0, 0, NULL, s);
	archive_string_init(&t);
	assertExactString(0, 0, NULL, t);
	archive_string_init(&u);
	assertExactString(0, 0, NULL, u);
	archive_string_init(&v);
	assertExactString(0, 0, NULL, v);

	/* null target, null source */
	archive_string_copy(&t, &s);
	assertExactString(0, 0, NULL, s);
	assertExactString(0, EXTENT, "", t);

	/* null target, empty source */
	archive_string_copy(&u, &t);
	assertExactString(0, EXTENT, "", t);
	assertExactString(0, EXTENT, "", u);

	/* empty target, empty source */
	archive_string_copy(&u, &t);
	assertExactString(0, EXTENT, "", t);
	assertExactString(0, EXTENT, "", u);

	/* null target, non-empty source */
	assert(NULL != archive_strcpy(&s, "snafubar"));
	assertExactString(8, EXTENT, "snafubar", s);

	archive_string_copy(&v, &s);
	assertExactString(8, EXTENT, "snafubar", s);
	assertExactString(8, EXTENT, "snafubar", v);

	/* empty target, non-empty source */
	assertExactString(0, EXTENT, "", t);
	archive_string_copy(&t, &s);
	assertExactString(8, EXTENT, "snafubar", s);
	assertExactString(8, EXTENT, "snafubar", t);

	/* non-empty target, non-empty source */
	assert(NULL != archive_strcpy(&s, "fubar"));
	assertExactString(5, EXTENT, "fubar", s);

	archive_string_copy(&t, &s);
	assertExactString(5, EXTENT, "fubar", s);
	assertExactString(5, EXTENT, "fubar", t);
}

DEFINE_TEST(test_archive_string)
{
	test_archive_string_ensure();
	test_archive_strcat();
	test_archive_strappend_char();
	test_archive_strncat();
	test_archive_strcpy();
	test_archive_string_concat();
	test_archive_string_copy();
}
