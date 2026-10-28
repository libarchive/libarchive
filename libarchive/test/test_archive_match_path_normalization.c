/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 libarchive contributors
 * All rights reserved.
 */

#include "test.h"

#include <locale.h>

/*
 * Exclusion patterns are also matched in Unicode Form D, so a name that
 * differs from the pattern only in canonical decomposition is excluded.
 * Filesystems such as APFS resolve both spellings to one file.
 *
 * Form C writes the accented "e" as U+00E9.  Form D writes it as "e"
 * followed by U+0301 COMBINING ACUTE ACCENT.
 */
#define NFC_MBS		"caf\xC3\xA9.txt"
#define NFD_MBS		"cafe\xCC\x81.txt"

/*
 * Matching reads the pattern and the pathname as UTF-8 regardless of the
 * locale.  Windows matches on the wide pathname, so the test text has to
 * survive the conversion from multibyte first.
 */
static int
have_utf8_locale(void)
{
	return (setlocale(LC_ALL, "en_US.UTF-8") != NULL ||
	    setlocale(LC_ALL, "C.UTF-8") != NULL ||
	    setlocale(LC_ALL, "UTF-8") != NULL);
}

/* With normalization disabled, matching is byte exact. */
static void
test_disabled_is_byte_exact(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 0));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFC_MBS));
	archive_entry_copy_pathname(ae, NFC_MBS);
	assertEqualInt(1, archive_match_path_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, NFD_MBS);
	failure("disabled: NFD must not match an NFC pattern");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* By default an NFC pattern catches the NFD entry. */
static void
test_exclusion_normalized(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFC_MBS));

	archive_entry_copy_pathname(ae, NFD_MBS);
	failure("NFD entry must be excluded by an NFC pattern");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	/* The exact form still matches. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, NFC_MBS);
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	/* An unrelated name is still not excluded. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "other.txt");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* An NFD pattern catches an NFC entry. */
static void
test_exclusion_normalized_reverse(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFD_MBS));

	archive_entry_copy_pathname(ae, NFC_MBS);
	failure("NFC entry must be excluded by an NFD pattern");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* The setting takes effect at match time. */
static void
test_toggle_after_add(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFC_MBS));
	archive_entry_copy_pathname(ae, NFD_MBS);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 0));
	failure("disabled after add must be byte exact");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	failure("enabled after add must normalize");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* Inclusion patterns are always byte exact. */
static void
test_inclusion_byte_exact(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_include_pattern(m, NFC_MBS));

	archive_entry_copy_pathname(ae, NFD_MBS);
	failure("an inclusion must not select a different spelling");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	/* The named form is included. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, NFC_MBS);
	assertEqualInt(0, archive_match_path_excluded(m, ae));
	assertEqualInt(0, archive_match_path_unmatched_inclusions(m));

	archive_match_free(m);

	/* Same for recursive inclusion of a directory. */
	assert((m = archive_match_new()) != NULL);
	assertEqualIntA(m, 0, archive_match_include_pattern(m, "caf\xC3\xA9"));

	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "cafe\xCC\x81/child.txt");
	failure("a recursive inclusion must not select another spelling");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* Directory prefix exclusion is normalized too. */
static void
test_directory_prefix_normalized(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "priv\xC3\xA9"));

	archive_entry_copy_pathname(ae, "prive\xCC\x81/inside.txt");
	failure("entry under an NFD directory must be excluded");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* A wildcard pattern with a non-ASCII literal matches. */
static void
test_wildcard_normalized(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "caf\xC3\xA9*"));

	archive_entry_copy_pathname(ae, NFD_MBS);
	failure("wildcard with an NFC literal must catch an NFD path");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/*
 * Normalization is canonical only.  Compatibility forms and case are
 * distinct names on case sensitive filesystems and must stay distinct.
 *
 *   U+00DF folds to "ss" by case folding only.  It has no decomposition.
 *   U+FB01 decomposes to "fi" by compatibility only.
 */
static void
test_canonical_only_negations(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "ss.txt"));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "fi.txt"));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "exclude.txt"));

	/* Case folding: U+00DF against "ss". */
	archive_entry_copy_pathname(ae, "\xC3\x9F.txt");
	failure("sharp s must not match ss");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	/* Compatibility: U+FB01 against "fi". */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "\xEF\xAC\x81.txt");
	failure("fi ligature must not match fi");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	/* Case. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "EXCLUDE.txt");
	failure("matching must remain case sensitive");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	/*
	 * The cases above stop at the byte comparison because one side is
	 * ASCII.  This pair keeps both sides non-ASCII so the Form D
	 * comparison runs.  U+FEFB has a compatibility decomposition to
	 * U+0644 U+0627 and no canonical one.
	 */
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m,
	    "\xD9\x84\xD8\xA7.txt"));			/* U+0644 U+0627 */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "\xEF\xBB\xBB.txt");	/* U+FEFB */
	failure("a ligature must not match its NFKC expansion");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* Combining marks are put in canonical order. */
static void
test_mark_order_normalized(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	/* q + U+0301 + U+0327, the two marks out of canonical order. */
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m,
	    "q\xCC\x81\xCC\xA7.txt"));

	/* The same two marks in canonical order. */
	archive_entry_copy_pathname(ae, "q\xCC\xA7\xCC\x81.txt");
	failure("marks in either order must be the same name");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* Two different non-ASCII names must stay different. */
static void
test_distinct_nonascii_no_overmatch(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFC_MBS));

	/* A different accented name, in Form D. */
	archive_entry_copy_pathname(ae, "nai\xCC\x88ve.txt");
	failure("a different accented name must not be excluded");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/*
 * A pattern deep enough to hit the matcher's recursion limit must give the
 * same answer as byte exact matching.  The matcher must stay usable.
 */
static void
test_deep_pattern_no_error(void)
{
	struct archive_entry *ae;
	struct archive *m;
	char pat[512], path[768];
	int i;

	strcpy(pat, NFC_MBS);
	for (i = 0; i < 200; i++)
		strcat(pat, "*a");
	strcpy(path, NFD_MBS);
	for (i = 0; i < 600; i++)
		strcat(path, "a");

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, pat));

	archive_entry_copy_pathname(ae, path);
	failure("a deep pattern must not report an error");
	assert(archive_match_path_excluded(m, ae) >= 0);

	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "plainfile.txt");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* ASCII patterns and pathnames match exactly as before. */
static void
test_ascii_fast_path_unchanged(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "^aa*"));

	archive_entry_copy_pathname(ae, "aa1234");
	assertEqualInt(1, archive_match_path_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "a1234");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/*
 * Invalid UTF-8 is matched byte for byte.  Two different invalid byte
 * sequences must stay different.
 */
static void
test_invalid_utf8_no_overmatch(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "sec\x80.txt"));

	archive_entry_copy_pathname(ae, "sec\x81.txt");
	failure("distinct invalid names must not match");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

#if !defined(_WIN32) || defined(__CYGWIN__)
	/* Identical bytes still match.  Windows matches on the wide pathname
	 * and these bytes have no wide form. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "sec\x80.txt");
	assertEqualInt(1, archive_match_path_excluded(m, ae));
#endif

	archive_entry_free(ae);
	archive_match_free(m);
}

#if !defined(_WIN32) || defined(__CYGWIN__)
/* Matching does not depend on the locale. */
static void
test_c_locale(void)
{
	struct archive_entry *ae;
	struct archive *m;

	if (NULL == setlocale(LC_ALL, "C")) {
		skipping("C locale is unavailable");
		return;
	}

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFC_MBS));

	archive_entry_copy_pathname(ae, NFD_MBS);
	failure("matching must not depend on the locale");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}
#endif

DEFINE_TEST(test_archive_match_path_normalization)
{
	int utf8 = have_utf8_locale();

#if defined(_WIN32) && !defined(__CYGWIN__)
	if (utf8 == 0) {
		skipping("Wide matching needs a UTF-8 locale");
		return;
	}
#else
	(void)utf8; /* UNUSED */
#endif

	test_disabled_is_byte_exact();
	test_exclusion_normalized();
	test_exclusion_normalized_reverse();
	test_toggle_after_add();
	test_inclusion_byte_exact();
	test_directory_prefix_normalized();
	test_wildcard_normalized();
	test_canonical_only_negations();
	test_mark_order_normalized();
	test_distinct_nonascii_no_overmatch();
	test_deep_pattern_no_error();
	test_ascii_fast_path_unchanged();
	test_invalid_utf8_no_overmatch();
#if !defined(_WIN32) || defined(__CYGWIN__)
	test_c_locale();
#endif
}

DEFINE_TEST(test_archive_match_path_normalization_brackets)
{
	static const struct {
		const char *pattern;
		const char *pathname;
		int excluded;
		int normalized;
	} tests[] = {
		{ "[\xC3\xA9]*.txt", "e\xC3\xB1.txt", 0, 0 },
		{ "[\xC3\xA9]*.txt", "\xC3\xA9.txt", 1, 1 },
		{ "[!\xC3\xA9]*.txt", "e\xC3\xB1.txt", 1, 1 },
		{ "caf\xC3\xA9[\xC3\xA9]*.txt",
		    "cafe\xCC\x81\xC3\xA9.txt", 0, 0 },
		{ "[\xC3\xA0-\xC3\xA9]*.txt", "e\xC3\xB1.txt", 0, 0 },
		{ "[a-z]*.txt", "\xC3\xA9.txt", 0, 1 },
		{ "[a-z]*.txt", "apple\xC3\xA9.txt", 1, 1 },
		{ "[!e]*.txt", "\xC3\xA9.txt", 1, 1 },
		{ "[^e]*.txt", "\xC3\xA9.txt", 1, 1 },
		{ "[a][\xC3\xA9]*.txt", "ae\xC3\xB1.txt", 0, 0 },
		{ "[\\]\xC3\xA9]*.txt", "e\xC3\xB1.txt", 0, 0 },
		{ "\\\\[\xC3\xA9]*.txt", "\\e\xC3\xB1.txt", 0, 0 },
		{ "[[]caf\xC3\xA9", "[cafe\xCC\x81", 0, 1 },
		{ "[]*", NFC_MBS, 0, 0 },
		{ "[!]*", NFC_MBS, 1, 1 },
		{ "caf\xC3\xA9[ab]", "caf\xC3\xA9" "a", 1, 1 },
		{ "caf\xC3\xA9[ab]", "cafe\xCC\x81" "a", 0, 1 },
		{ "caf\xC3\xA9/[0-9].txt", "cafe\xCC\x81/1.txt", 0, 1 },
		{ "caf\xC3\xA9/[!x].txt", "cafe\xCC\x81/y.txt", 0, 1 },
		{ "caf\xC3\xA9/[^x].txt", "cafe\xCC\x81/y.txt", 0, 1 },
		{ "[\\]]caf\xC3\xA9", "]cafe\xCC\x81", 0, 1 },
		{ "[a][b]caf\xC3\xA9", "abcafe\xCC\x81", 0, 1 },
		{ "\\[caf\xC3\xA9]", "[cafe\xCC\x81]", 0, 1 },
		{ "[caf\xC3\xA9", "[cafe\xCC\x81", 0, 1 },
		{ "[caf\xC3\xA9\\]", "[cafe\xCC\x81]", 0, 1 },
		{ "caf\xC3\xA9[\\", "cafe\xCC\x81[\\", 0, 1 },
		{ "caf\xC3\xA9*", NFD_MBS, 0, 1 }
	};
	struct archive_entry *ae;
	struct archive *m;
	size_t i;
	int j, enabled;

#if defined(_WIN32) && !defined(__CYGWIN__)
	if (!have_utf8_locale()) {
		skipping("Wide matching needs a UTF-8 locale");
		return;
	}
#endif
	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		assert((m = archive_match_new()) != NULL);
		assert((ae = archive_entry_new()) != NULL);
		assertEqualIntA(m, ARCHIVE_OK,
		    archive_match_exclude_pattern(m, tests[i].pattern));
		archive_entry_copy_pathname(ae, tests[i].pathname);
		for (j = 0; j < 3; j++) {
			enabled = j != 1;
			assertEqualIntA(m, ARCHIVE_OK,
			    archive_match_set_pattern_normalization(m, enabled));
			failure("pattern %s, pathname %s, normalization %d",
			    tests[i].pattern, tests[i].pathname, enabled);
			assertEqualIntA(m, enabled ? tests[i].normalized :
			    tests[i].excluded, archive_match_path_excluded(m, ae));
		}
		archive_entry_free(ae);
		archive_match_free(m);
	}

	/* Original matches still apply after a normalization skip. */
	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);
	assertEqualIntA(m, ARCHIVE_OK,
	    archive_match_exclude_pattern(m, "[\xC3\xA9]*.txt"));
	archive_entry_copy_pathname(ae, "e\xC3\xB1.txt");
	assertEqualIntA(m, 0, archive_match_path_excluded(m, ae));
	archive_entry_copy_pathname(ae, "\xC3\xA9.txt");
	assertEqualIntA(m, 1, archive_match_path_excluded(m, ae));
	archive_entry_copy_pathname(ae, "e\xC3\xB1.txt");
	assertEqualIntA(m, 0, archive_match_path_excluded(m, ae));
	archive_entry_free(ae);
	archive_match_free(m);
}

DEFINE_TEST(test_archive_match_path_normalization_brackets_wide)
{
	static const struct {
		const wchar_t *pattern;
		const wchar_t *pathname;
		int excluded;
		int normalized;
	} tests[] = {
		{ L"[\x00E9]*.txt", L"e\x00F1.txt", 0, 0 },
		{ L"[\x00E9]*.txt", L"\x00E9.txt", 1, 1 },
		{ L"[!\x00E9]*.txt", L"e\x00F1.txt", 1, 1 },
		{ L"caf\x00E9[\x00E9]*.txt",
		    L"cafe\x0301\x00E9.txt", 0, 0 },
#if defined(_WIN32) && !defined(__CYGWIN__)
		{ L"[\x03B2]*caf\x00E9", L"\x03B3" L"caf\x00E9", 0, 0 },
#endif
		{ L"[\x00E0-\x00E9]*.txt", L"e\x00F1.txt", 0, 0 },
		{ L"[a-z]*.txt", L"\x00E9.txt", 0, 1 },
		{ L"[!e]*.txt", L"\x00E9.txt", 1, 1 },
		{ L"[^e]*.txt", L"\x00E9.txt", 1, 1 },
		{ L"[\\]\x00E9]*.txt", L"e\x00F1.txt", 0, 0 },
		{ L"\\\\[\x00E9]*.txt", L"\\e\x00F1.txt", 0, 0 },
		{ L"[[]caf\x00E9", L"[cafe\x0301", 0, 1 },
		{ L"caf\x00E9[ab]", L"cafe\x0301" L"a", 0, 1 },
		{ L"caf\x00E9/[0-9].txt", L"cafe\x0301/1.txt", 0, 1 },
		{ L"caf\x00E9/[!x].txt", L"cafe\x0301/y.txt", 0, 1 },
		{ L"caf\x00E9/[^x].txt", L"cafe\x0301/y.txt", 0, 1 },
		{ L"[\\]]caf\x00E9", L"]cafe\x0301", 0, 1 },
		{ L"[a][b]caf\x00E9", L"abcafe\x0301", 0, 1 },
		{ L"\\[caf\x00E9]", L"[cafe\x0301]", 0, 1 },
		{ L"[caf\x00E9", L"[cafe\x0301", 0, 1 },
		{ L"[caf\x00E9\\]", L"[cafe\x0301]", 0, 1 },
		{ L"caf\x00E9[\\", L"cafe\x0301[\\", 0, 1 },
		{ L"caf\x00E9*", L"cafe\x0301.txt", 0, 1 }
	};
	struct archive_entry *ae;
	struct archive *m;
	size_t i;
	int j, enabled;

#if !defined(_WIN32) || defined(__CYGWIN__)
	if (!have_utf8_locale()) {
		skipping("Wide patterns need a UTF-8 locale");
		return;
	}
#endif
	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		assert((m = archive_match_new()) != NULL);
		assert((ae = archive_entry_new()) != NULL);
		assertEqualIntA(m, ARCHIVE_OK,
		    archive_match_exclude_pattern_w(m, tests[i].pattern));
		archive_entry_copy_pathname_w(ae, tests[i].pathname);
		for (j = 0; j < 3; j++) {
			enabled = j != 1;
			assertEqualIntA(m, ARCHIVE_OK,
			    archive_match_set_pattern_normalization(m, enabled));
			failure("pattern %ls, pathname %ls, normalization %d",
			    tests[i].pattern, tests[i].pathname, enabled);
			assertEqualIntA(m, enabled ? tests[i].normalized :
			    tests[i].excluded, archive_match_path_excluded(m, ae));
		}
		archive_entry_free(ae);
		archive_match_free(m);
	}

	/* Original matches still apply after a normalization skip. */
	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);
	assertEqualIntA(m, ARCHIVE_OK,
	    archive_match_exclude_pattern_w(m, L"[\x00E9]*.txt"));
	archive_entry_copy_pathname_w(ae, L"e\x00F1.txt");
	assertEqualIntA(m, 0, archive_match_path_excluded(m, ae));
	archive_entry_copy_pathname_w(ae, L"\x00E9.txt");
	assertEqualIntA(m, 1, archive_match_path_excluded(m, ae));
	archive_entry_copy_pathname_w(ae, L"e\x00F1.txt");
	assertEqualIntA(m, 0, archive_match_path_excluded(m, ae));
	archive_entry_free(ae);
	archive_match_free(m);
}
