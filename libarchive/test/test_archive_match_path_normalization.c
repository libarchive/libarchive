/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 libarchive contributors
 * All rights reserved.
 */

#include "test.h"

#include <locale.h>

/*
 * archive_match_set_pattern_normalization() also compares exclusion patterns
 * and pathnames in Unicode Form D, closing the bypass described in
 * CVE-2026-45110 on filesystems that fold normalization, such as APFS.
 *
 * "cafe" plus an acute accent is written two ways that are canonically
 * equivalent but not byte equal: Form C uses the precomposed U+00E9, Form D
 * uses a bare 'e' followed by U+0301 COMBINING ACUTE ACCENT.  A byte
 * comparison treats them as different names; the filesystem does not.
 */
#define NFC_MBS		"caf\xC3\xA9.txt"
#define NFD_MBS		"cafe\xCC\x81.txt"

/*
 * Matching does not depend on the locale: the pattern and the pathname are
 * read as UTF-8.  Windows matches on the wide pathname though, so there the
 * test text has to survive the conversion from mbs first.
 */
static int
have_utf8_locale(void)
{
	return (setlocale(LC_ALL, "en_US.UTF-8") != NULL ||
	    setlocale(LC_ALL, "C.UTF-8") != NULL ||
	    setlocale(LC_ALL, "UTF-8") != NULL);
}

/*
 * Backward compatibility: with normalization left at its default, matching
 * stays byte exact and an NFD entry is not caught by an NFC pattern.
 */
static void
test_default_off_is_byte_exact(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFC_MBS));
	archive_entry_copy_pathname(ae, NFC_MBS);
	assertEqualInt(1, archive_match_path_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, NFD_MBS);
	failure("default is byte exact: NFD must not match an NFC pattern");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* An NFC pattern must catch the NFD entry, which is the reported bypass. */
static void
test_exclusion_normalized(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFC_MBS));

	archive_entry_copy_pathname(ae, NFD_MBS);
	failure("NFD entry must be excluded by an NFC pattern");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	/* Exact forms still match with the flag on. */
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

/* Reverse direction: an NFD pattern must catch an NFC entry.  A fix that
 * normalized only the pathname would fail this. */
static void
test_exclusion_normalized_reverse(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFD_MBS));

	archive_entry_copy_pathname(ae, NFC_MBS);
	failure("NFC entry must be excluded by an NFD pattern");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* The setting is read at match time, so it may be set after the pattern. */
static void
test_enable_after_add(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFC_MBS));
	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));

	archive_entry_copy_pathname(ae, NFD_MBS);
	failure("normalization enabled after add must still apply");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/*
 * Inclusions stay byte exact even with the flag on, so an inclusion never
 * selects more than the user asked for.  Excluding may over-match safely,
 * including may not.
 */
static void
test_inclusion_byte_exact(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	assertEqualIntA(m, 0, archive_match_include_pattern(m, NFC_MBS));

	archive_entry_copy_pathname(ae, NFD_MBS);
	failure("an inclusion must not pick up a different spelling");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	/* The named form is still included. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, NFC_MBS);
	assertEqualInt(0, archive_match_path_excluded(m, ae));
	assertEqualInt(0, archive_match_path_unmatched_inclusions(m));

	archive_match_free(m);

	/* Same for recursive inclusion of a directory. */
	assert((m = archive_match_new()) != NULL);
	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	assertEqualIntA(m, 0, archive_match_include_pattern(m, "caf\xC3\xA9"));

	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "cafe\xCC\x81/child.txt");
	failure("a recursive inclusion must not pick up another spelling");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* Directory prefix exclusion is not anchored, and is normalized too. */
static void
test_directory_prefix_normalized(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "priv\xC3\xA9"));

	archive_entry_copy_pathname(ae, "prive\xCC\x81/inside.txt");
	failure("entry under an NFD directory must be excluded");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* A wildcard pattern with a non-ASCII literal still matches. */
static void
test_wildcard_normalized(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "caf\xC3\xA9*"));

	archive_entry_copy_pathname(ae, NFD_MBS);
	failure("wildcard with an NFC literal must catch an NFD path");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/*
 * Scope lock: normalization is canonical only.  It must not fold
 * compatibility forms or case, which are meaningful on the common case
 * sensitive filesystems.  Widening this to NFKC or case folding would make
 * matching over-inclusive and trip here.
 *
 *   U+00DF becomes "ss" by case folding only; it has no decomposition.
 *   U+FB01 becomes "fi" by compatibility, and is unchanged by Form D.
 */
static void
test_canonical_only_negations(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "ss.txt"));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "fi.txt"));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "exclude.txt"));

	/* U+00DF must not be caught by "ss": case folding is out of scope. */
	archive_entry_copy_pathname(ae, "\xC3\x9F.txt");
	failure("sharp s must not match ss");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	/* U+FB01 must not be caught by "fi": compatibility is out of scope. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "\xEF\xAC\x81.txt");
	failure("fi ligature must not match fi");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	/* Matching stays case sensitive. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "EXCLUDE.txt");
	failure("matching must remain case sensitive");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	/*
	 * The cases above short-circuit on the byte comparison because one
	 * side is ASCII.  This pair keeps both sides non-ASCII so the
	 * normalized comparison actually runs.  U+FEFB has a compatibility
	 * decomposition to U+0644 U+0627 but no canonical one, so Form D
	 * keeps them distinct.  NFKC would wrongly fold them.
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

/*
 * Combining marks are reordered into canonical order, so the same marks
 * written in either order are one name.
 */
static void
test_mark_order_normalized(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	/* q + U+0301 + U+0327, the two marks in non-canonical order. */
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m,
	    "q\xCC\x81\xCC\xA7.txt"));

	/* The same two marks in canonical order. */
	archive_entry_copy_pathname(ae, "q\xCC\xA7\xCC\x81.txt");
	failure("marks in either order must be the same name");
	assertEqualInt(1, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/*
 * Two distinct non-ASCII names must never collapse into one match.  This
 * drives the normalized comparison on both sides.
 */
static void
test_distinct_nonascii_no_overmatch(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, NFC_MBS));

	/* A different accented name, in Form D. */
	archive_entry_copy_pathname(ae, "nai\xCC\x88ve.txt");
	failure("a different accented name must not be over-excluded");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/*
 * A pattern deep enough to hit the matcher's recursion limit must give the
 * same answer with the flag on as without it.  The added comparison must not
 * turn a benign result into a fatal error, which would poison the matcher for
 * every later entry.
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
	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, pat));

	archive_entry_copy_pathname(ae, path);
	failure("a deep pattern must not report an error");
	assert(archive_match_path_excluded(m, ae) >= 0);

	/* The matcher must still be usable. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "plainfile.txt");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

	archive_entry_free(ae);
	archive_match_free(m);
}

/* Enabling normalization must not change any result for pure ASCII patterns
 * and pathnames, wildcards included. */
static void
test_ascii_fast_path_unchanged(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
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
 * Invalid UTF-8 keeps the byte comparison and must never over-match: two
 * distinct invalid byte sequences must not be treated as equal.  This is the
 * guarantee for archives whose names are not Unicode at all.
 */
static void
test_invalid_utf8_no_overmatch(void)
{
	struct archive_entry *ae;
	struct archive *m;

	assert((m = archive_match_new()) != NULL);
	assert((ae = archive_entry_new()) != NULL);

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
	assertEqualIntA(m, 0, archive_match_exclude_pattern(m, "sec\x80.txt"));

	/* A different invalid byte must not collapse to the same name. */
	archive_entry_copy_pathname(ae, "sec\x81.txt");
	failure("distinct invalid names must not over-match");
	assertEqualInt(0, archive_match_path_excluded(m, ae));

#if !defined(_WIN32) || defined(__CYGWIN__)
	/* The identical bytes still match.  Windows matches on the wide
	 * pathname, which these bytes have no conversion to. */
	archive_entry_clear(ae);
	archive_entry_copy_pathname(ae, "sec\x80.txt");
	assertEqualInt(1, archive_match_path_excluded(m, ae));
#endif

	archive_entry_free(ae);
	archive_match_free(m);
}

#if !defined(_WIN32) || defined(__CYGWIN__)
/*
 * The C locale is the usual environment for cron jobs and container builds.
 * Names are read as UTF-8 there too, so the exclusion still holds, and names
 * that are not valid UTF-8 still fall back to the byte comparison.
 */
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

	assertEqualIntA(m, 0, archive_match_set_pattern_normalization(m, 1));
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

	test_default_off_is_byte_exact();
	test_exclusion_normalized();
	test_exclusion_normalized_reverse();
	test_enable_after_add();
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
