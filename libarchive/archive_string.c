/*-
 * Copyright (c) 2003-2011 Tim Kientzle
 * Copyright (c) 2011 Michihiro NAKAJIMA
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

#include "archive_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_string.c 201095 2009-12-28 02:33:22Z kientzle $");

/*
 * Basic resizable string support, to simplify manipulating arbitrary-sized
 * strings while minimizing heap activity.
 *
 * In particular, the buffer used by a string object is only grown, it
 * never shrinks, so you can clear and reuse the same string object
 * without incurring additional memory allocations.
 */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_ICONV_H
#include <iconv.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_LOCALCHARSET_H
#include <localcharset.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#include <locale.h>
#endif

#include "archive_endian.h"
#include "archive_private.h"
#include "archive_string.h"


struct archive_string_conv {
	struct archive_string_conv	*next;
	char				*from_charset;
	char				*to_charset;
	unsigned			 from_cp;
	unsigned			 to_cp;
	/* Set 1 if from_charset and to_charset are the same. */
	int				 same;
	int				 flag;
#define SCONV_TO_CHARSET	1	/* MBS is being converted to specified
					 * charset. */
#define SCONV_FROM_CHARSET	2	/* MBS is being converted from
					 * specified charset. */
#define SCONV_BEST_EFFORT 	4	/* Copy at least ASCII code. */
#define SCONV_WIN_CP	 	8	/* Use Windows API for converting
					 * MBS. */
#define SCONV_UTF16BE	 	16	/* Consideration to UTF-16BE; one side
					 * is single byte character, other is
					 * double bytes character. */
#define SCONV_UTF8_LIBARCHIVE_2 32	/* Incorrect UTF-8 made by libarchive
					 * 2.x in the wrong assumption. */
#define SCONV_COPY_UTF8_TO_UTF8	64	/* Copy UTF-8 string in checking
					 * CESU-8. */

#if HAVE_ICONV
	iconv_t				 cd;
#endif
};

#define CP_C_LOCALE	0	/* "C" locale */

static struct archive_string_conv *find_sconv_object(struct archive *,
	const char *, const char *);
static void add_sconv_object(struct archive *, struct archive_string_conv *);
static struct archive_string_conv *create_sconv_object(const char *,
	const char *, unsigned, int);
static void free_sconv_object(struct archive_string_conv *);
static struct archive_string_conv *get_sconv_object(struct archive *,
	const char *, const char *, int);
static unsigned make_codepage_from_charset(const char *);
static unsigned get_current_codepage();
static unsigned get_current_oemcp();
static int strncpy_from_utf16be(struct archive_string *, const void *, size_t,
    struct archive_string_conv *);
static int strncpy_to_utf16be(struct archive_string *, const void *, size_t,
    struct archive_string_conv *);
static int best_effort_strncat_in_locale(struct archive_string *, const void *,
    size_t, struct archive_string_conv *);
static int strncat_from_utf8_libarchive2(struct archive_string *,
    const char *, size_t);
static int strncat_from_utf8_utf8(struct archive_string *, const char *,
    size_t);

static struct archive_string *
archive_string_append(struct archive_string *as, const char *p, size_t s)
{
	if (archive_string_ensure(as, as->length + s + 1) == NULL)
		__archive_errx(1, "Out of memory");
	memcpy(as->s + as->length, p, s);
	as->length += s;
	as->s[as->length] = 0;
	return (as);
}

static struct archive_wstring *
archive_wstring_append(struct archive_wstring *as, const wchar_t *p, size_t s)
{
	if (archive_wstring_ensure(as, as->length + s + 1) == NULL)
		__archive_errx(1, "Out of memory");
	memcpy(as->s + as->length, p, s * sizeof(wchar_t));
	as->length += s;
	as->s[as->length] = 0;
	return (as);
}

void
archive_string_concat(struct archive_string *dest, struct archive_string *src)
{
	archive_string_append(dest, src->s, src->length);
}

void
archive_wstring_concat(struct archive_wstring *dest, struct archive_wstring *src)
{
	archive_wstring_append(dest, src->s, src->length);
}

void
archive_string_free(struct archive_string *as)
{
	as->length = 0;
	as->buffer_length = 0;
	free(as->s);
	as->s = NULL;
}

void
archive_wstring_free(struct archive_wstring *as)
{
	as->length = 0;
	as->buffer_length = 0;
	free(as->s);
	as->s = NULL;
}

struct archive_wstring *
archive_wstring_ensure(struct archive_wstring *as, size_t s)
{
	return (struct archive_wstring *)
		archive_string_ensure((struct archive_string *)as,
					s * sizeof(wchar_t));
}

/* Returns NULL on any allocation failure. */
struct archive_string *
archive_string_ensure(struct archive_string *as, size_t s)
{
	char *p;
	size_t new_length;

	/* If buffer is already big enough, don't reallocate. */
	if (as->s && (s <= as->buffer_length))
		return (as);

	/*
	 * Growing the buffer at least exponentially ensures that
	 * append operations are always linear in the number of
	 * characters appended.  Using a smaller growth rate for
	 * larger buffers reduces memory waste somewhat at the cost of
	 * a larger constant factor.
	 */
	if (as->buffer_length < 32)
		/* Start with a minimum 32-character buffer. */
		new_length = 32;
	else if (as->buffer_length < 8192)
		/* Buffers under 8k are doubled for speed. */
		new_length = as->buffer_length + as->buffer_length;
	else {
		/* Buffers 8k and over grow by at least 25% each time. */
		new_length = as->buffer_length + as->buffer_length / 4;
		/* Be safe: If size wraps, fail. */
		if (new_length < as->buffer_length) {
			/* On failure, wipe the string and return NULL. */
			archive_string_free(as);
			return (NULL);
		}
	}
	/*
	 * The computation above is a lower limit to how much we'll
	 * grow the buffer.  In any case, we have to grow it enough to
	 * hold the request.
	 */
	if (new_length < s)
		new_length = s;
	/* Now we can reallocate the buffer. */
	p = (char *)realloc(as->s, new_length);
	if (p == NULL) {
		/* On failure, wipe the string and return NULL. */
		archive_string_free(as);
		return (NULL);
	}

	as->s = p;
	as->buffer_length = new_length;
	return (as);
}

/*
 * TODO: See if there's a way to avoid scanning
 * the source string twice.  Then test to see
 * if it actually helps (remember that we're almost
 * always called with pretty short arguments, so
 * such an optimization might not help).
 */
struct archive_string *
archive_strncat(struct archive_string *as, const void *_p, size_t n)
{
	size_t s;
	const char *p, *pp;

	p = (const char *)_p;

	/* Like strlen(p), except won't examine positions beyond p[n]. */
	s = 0;
	pp = p;
	while (s < n && *pp) {
		pp++;
		s++;
	}
	return (archive_string_append(as, p, s));
}

struct archive_wstring *
archive_wstrncat(struct archive_wstring *as, const wchar_t *p, size_t n)
{
	size_t s;
	const wchar_t *pp;

	/* Like strlen(p), except won't examine positions beyond p[n]. */
	s = 0;
	pp = p;
	while (s < n && *pp) {
		pp++;
		s++;
	}
	return (archive_wstring_append(as, p, s));
}

struct archive_string *
archive_strcat(struct archive_string *as, const void *p)
{
	/* strcat is just strncat without an effective limit. 
	 * Assert that we'll never get called with a source
	 * string over 16MB.
	 * TODO: Review all uses of strcat in the source
	 * and try to replace them with strncat().
	 */
	return archive_strncat(as, p, 0x1000000);
}

struct archive_wstring *
archive_wstrcat(struct archive_wstring *as, const wchar_t *p)
{
	/* Ditto. */
	return archive_wstrncat(as, p, 0x1000000);
}

struct archive_string *
archive_strappend_char(struct archive_string *as, char c)
{
	return (archive_string_append(as, &c, 1));
}

struct archive_wstring *
archive_wstrappend_wchar(struct archive_wstring *as, wchar_t c)
{
	return (archive_wstring_append(as, &c, 1));
}

/*
 * Get the "current character set" name to use with iconv.
 * On FreeBSD, the empty character set name "" chooses
 * the correct character encoding for the current locale,
 * so this isn't necessary.
 * But iconv on Mac OS 10.6 doesn't seem to handle this correctly;
 * on that system, we have to explicitly call nl_langinfo()
 * to get the right name.  Not sure about other platforms.
 *
 * NOTE: GNU libiconv does not recognize the character-set name
 * which some platform nl_langinfo(CODESET) returns, so we should
 * use locale_charset() instead of nl_langinfo(CODESET) for GNU libiconv.
 */
static const char *
default_iconv_charset(const char *charset) {
	if (charset != NULL && charset[0] != '\0')
		return charset;
#if HAVE_LOCALE_CHARSET && !defined(__APPLE__)
	/* locale_charset() is broken on Mac OS */
	return locale_charset();
#elif HAVE_NL_LANGINFO
	return nl_langinfo(CODESET);
#else
	return "";
#endif
}

#if defined(_WIN32) && !defined(__CYGWIN__)

/*
 * Convert MBS to WCS.
 * Note: returns -1 if conversion fails.
 */
int
archive_wstring_append_from_mbs(struct archive_wstring *dest,
    const char *p, size_t len)
{
	size_t r;
	unsigned cp;
	/*
	 * No single byte will be more than one wide character,
	 * so this length estimate will always be big enough.
	 */
	size_t wcs_length = len;
	if (NULL == archive_wstring_ensure(dest, dest->length + wcs_length + 1))
		__archive_errx(1,
		    "No memory for archive_wstring_append_from_mbs()");

	cp = get_current_codepage();
	if (cp == CP_C_LOCALE) {
		wchar_t *wp = dest->s + dest->length;
		const unsigned char *mp = (const unsigned char *)p;

		r = 0;
		while (r < len && *mp) {
			*wp++ = (wchar_t)*mp++;
			r++;
		}
	} else {
		r = MultiByteToWideChar(cp, 0,
		    p, (int)len, dest->s + dest->length, (int)wcs_length);
	}
	if (r > 0) {
		dest->length += r;
		dest->s[dest->length] = 0;
		return (0);
	}
	return (-1);
}

#else

/*
 * Convert MBS to WCS.
 * Note: returns -1 if conversion fails.
 */
int
archive_wstring_append_from_mbs(struct archive_wstring *dest,
    const char *p, size_t len)
{
	size_t r;
	/*
	 * No single byte will be more than one wide character,
	 * so this length estimate will always be big enough.
	 */
	size_t wcs_length = len;
	size_t mbs_length = len;
	const char *mbs = p;
	wchar_t *wcs;
#if HAVE_MBRTOWC || HAVE_MBSNRTOWCS
	mbstate_t shift_state;

	memset(&shift_state, 0, sizeof(shift_state));
#endif
	if (NULL == archive_wstring_ensure(dest, dest->length + wcs_length + 1))
		__archive_errx(1,
		    "No memory for archive_wstring_append_from_mbs()");
	wcs = dest->s + dest->length;
#if HAVE_MBSNRTOWCS
	r = mbsnrtowcs(wcs, &mbs, mbs_length, wcs_length, &shift_state);
	if (r != (size_t)-1) {
		dest->length += r;
		dest->s[dest->length] = L'\0';
		return (0);
	}
	return (-1);
#else /* HAVE_MBSNRTOWCS */
	/*
	 * We cannot use mbsrtowcs/mbstowcs here because those may convert
	 * extra MBS when strlen(p) > len and one wide character consis of
	 * multi bytes.
	 */
	while (wcs_length > 0 && *mbs && mbs_length > 0) {
#if HAVE_MBRTOWC
		r = mbrtowc(wcs, mbs, wcs_length, &shift_state);
#else
		r = mbtowc(wcs, mbs, wcs_length);
#endif
		if (r == (size_t)-1 || r == (size_t)-2) {
			dest->s[dest->length] = L'\0';
			return (-1);
		}
		if (r == 0 || r > mbs_length)
			break;
		wcs++;
		wcs_length--;
		mbs += r;
		mbs_length -= r;
	}
	dest->length = wcs - dest->s;
	dest->s[dest->length] = L'\0';
	return (0);
#endif /* HAVE_MBSNRTOWCS */
}

#endif

#if defined(_WIN32) && !defined(__CYGWIN__)

/*
 * WCS ==> MBS.
 * Note: returns -1 if conversion fails.
 *
 * Win32 builds use WideCharToMultiByte from the Windows API.
 * (Maybe Cygwin should too?  WideCharToMultiByte will know a
 * lot more about local character encodings than the wcrtomb()
 * wrapper is going to know.)
 */
int
archive_string_append_from_wcs(struct archive_string *as,
    const wchar_t *w, size_t len)
{
	int l;
	unsigned cp;
	BOOL useDefaultChar;

	cp = get_current_codepage();
	if (cp == CP_C_LOCALE) {
		/*
		 * "C" locale special process.
		 */
		char *p;
		archive_string_ensure(as, as->length + len + 1);
		p = as->s + as->length;
		l = 0;
		while (l < (int)len && *w) {
			if (*w > 255)
				return (-1);
			*p++ = (char)*w++;
			l++;
		}
		*p = '\0';
		as->length += l;
		return (0);
	}

	/* Make sure the MBS buffer has plenty to set. */
	archive_string_ensure(as, as->length + len * 2 + 1);
	do {
		useDefaultChar = FALSE;
		l = WideCharToMultiByte(cp, 0, w, len, as->s + as->length,
		    as->buffer_length-1, NULL, &useDefaultChar);
		if (l == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
			/* Expand the MBS buffer and retry. */
			archive_string_ensure(as, as->buffer_length + len);
			continue;
		}
	} while (0);

	if (l == 0 || useDefaultChar) {
		/* Conversion error happend. */
		as->s[as->length] = '\0';
		return (-1);
	}
	as->length += l;
	as->s[as->length] = '\0';
	return (0);
}

#elif defined(HAVE_WCSNRTOMBS)

/*
 * Translates a wide character string into current locale character set
 * and appends to the archive_string.  Note: returns -1 if conversion
 * fails.
 */
int
archive_string_append_from_wcs(struct archive_string *as,
    const wchar_t *w, size_t len)
{
	mbstate_t shift_state;
	size_t r, ndest, nwc;
	char *dest;
	const wchar_t *wp, *wpp;

	wp = w;
	nwc = len;
	ndest = len * 2;
	/* Initialize the shift state. */
	memset(&shift_state, 0, sizeof(shift_state));
	for (;;) {
		/* Allocate buffer for MBS. */
		if (archive_string_ensure(as, as->length + ndest + 1) == NULL)
			__archive_errx(1, "Out of memory");

		dest = as->s + as->length;
		wpp = wp;
		r = wcsnrtombs(dest, &wp, nwc,
		    as->buffer_length - as->length -1,
		    &shift_state);
		if (r == (size_t)-1)
			return (-1);
		as->length += r;
		if (wp == NULL || (wp - wpp) >= nwc) {
			/* All wide characters are translated to MBS. */
			as->s[as->length] = '\0';
			return (0);
		}
		/* Get a remaining WCS lenth. */
		nwc -= wp - wpp;
	}
}

#elif defined(HAVE_WCTOMB) || defined(HAVE_WCRTOMB)

/*
 * Translates a wide character string into current locale character set
 * and appends to the archive_string.  Note: returns -1 if conversion
 * fails.
 */
int
archive_string_append_from_wcs(struct archive_string *as,
    const wchar_t *w, size_t len)
{
	/* We cannot use the standard wcstombs() here because it
	 * cannot tell us how big the output buffer should be.  So
	 * I've built a loop around wcrtomb() or wctomb() that
	 * converts a character at a time and resizes the string as
	 * needed.  We prefer wcrtomb() when it's available because
	 * it's thread-safe. */
	int n;
	char *p;
	char *end;
#if HAVE_WCRTOMB
	mbstate_t shift_state;

	memset(&shift_state, 0, sizeof(shift_state));
#else
	/* Clear the shift state before starting. */
	wctomb(NULL, L'\0');
#endif
	/*
	 * Allocate buffer for MBS.
	 * We need this allocation here since it is possible that
	 * as->s is still NULL.
	 */
	if (archive_string_ensure(as, as->length + len + 1) == NULL)
		__archive_errx(1, "Out of memory");

	p = as->s + as->length;
	end = as->s + as->buffer_length - MB_CUR_MAX -1;
	while (*w != L'\0' && len > 0) {
		if (p >= end) {
			as->length = p - as->s;
			/* Re-allocate buffer for MBS. */
			if (archive_string_ensure(as,
			    as->length + len * 2 + 1) == NULL)
				__archive_errx(1, "Out of memory");
			p = as->s + as->length;
			end = as->s + as->buffer_length - MB_CUR_MAX -1;
		}
#if HAVE_WCRTOMB
		n = wcrtomb(p, *w++, &shift_state);
#else
		n = wctomb(p, *w++);
#endif
		if (n == -1)
			return (-1);
		p += n;
		len--;
	}
	as->length = p - as->s;
	as->s[as->length] = '\0';
	return (0);
}

#else /* HAVE_WCTOMB || HAVE_WCRTOMB */

/*
 * TODO: Test if __STDC_ISO_10646__ is defined.
 * Non-Windows uses ISO C wcrtomb() or wctomb() to perform the conversion
 * one character at a time.  If a non-Windows platform doesn't have
 * either of these, fall back to the built-in UTF8 conversion.
 */
int
archive_string_append_from_wcs(struct archive_string *as,
    const wchar_t *w, size_t len)
{
	(void)as;/* UNUSED */
	(void)w;/* UNUSED */
	(void)len;/* UNUSED */
	return (-1);
}

#endif /* HAVE_WCTOMB || HAVE_WCRTOMB */

/*
 * Find a string conversion object by a pair of 'from' charset name
 * and 'to' charset name from an archive object.
 * Return NULL if not found.
 */
static struct archive_string_conv *
find_sconv_object(struct archive *a, const char *fc, const char *tc)
{
	struct archive_string_conv *sc; 

	if (a == NULL)
		return (NULL);

	for (sc = a->sconv; sc != NULL; sc = sc->next) {
		if (strcmp(sc->from_charset, fc) == 0 &&
		    strcmp(sc->to_charset, tc) == 0)
			break;
	}
	return (sc);
}

/*
 * Register a string object to an archive object.
 */
static void
add_sconv_object(struct archive *a, struct archive_string_conv *sc)
{
	struct archive_string_conv **psc; 

	/* Add a new sconv to sconv list. */
	psc = &(a->sconv);
	while (*psc != NULL)
		psc = &((*psc)->next);
	*psc = sc;
}

/*
 * Create a string conversion object.
 */
static struct archive_string_conv *
create_sconv_object(const char *fc, const char *tc,
    unsigned current_codepage, int flag)
{
	struct archive_string_conv *sc; 

	/*
	 * Special conversion for the incorrect UTF-8 made by libarchive 2.x
	 * only for the platform WCS of which is not Unicode.
	 */
	if (strcmp(fc, "UTF-8-MADE_BY_LIBARCHIVE2") == 0)
#if (defined(_WIN32) && !defined(__CYGWIN__)) \
	 || defined(__STDC_ISO_10646__) || defined(__APPLE__)
		fc = "UTF-8";/* Ignore special sequence. */
#else
		flag |= SCONV_UTF8_LIBARCHIVE_2;
#endif

	sc = malloc(sizeof(*sc));
	if (sc == NULL)
		return (NULL);
	sc->next = NULL;
	sc->from_charset = strdup(fc);
	if (sc->from_charset == NULL) {
		free(sc);
		return (NULL);
	}
	sc->to_charset = strdup(tc);
	if (sc->to_charset == NULL) {
		free(sc);
		free(sc->from_charset);
		return (NULL);
	}

	if (flag & SCONV_UTF8_LIBARCHIVE_2) {
		sc->flag = flag;
		return (sc);
	}
#if HAVE_ICONV
	sc->cd = iconv_open(tc, fc);
#endif

	if (flag & SCONV_TO_CHARSET) {
		if (strcmp(tc, "UTF-16BE") == 0)
			flag |= SCONV_UTF16BE;
		sc->from_cp = current_codepage;
		sc->to_cp = make_codepage_from_charset(tc);
#if defined(_WIN32) && !defined(__CYGWIN__)
		if (IsValidCodePage(sc->to_cp))
			flag |= SCONV_WIN_CP;
#endif
	} else if (flag & SCONV_FROM_CHARSET) {
		if (strcmp(fc, "UTF-16BE") == 0)
			flag |= SCONV_UTF16BE;
		sc->to_cp = current_codepage;
		sc->from_cp = make_codepage_from_charset(fc);
#if defined(_WIN32) && !defined(__CYGWIN__)
		if (IsValidCodePage(sc->from_cp))
			flag |= SCONV_WIN_CP;
#endif
	}

	/*
	 * Check if "from charset" and "to charset" are the same.
	 */
	if (strcmp(fc, tc) == 0 ||
	    (sc->from_cp != -1 && sc->from_cp == sc->to_cp))
		sc->same = 1;
	else
		sc->same = 0;

	/*
	 * Copy UTF-8 string in checking CESU-8 including surrogate pair.
	 */
	if (sc->same && strcmp(fc, "UTF-8") == 0)
		flag |= SCONV_COPY_UTF8_TO_UTF8;

	sc->flag = flag;

	return (sc);
}

/*
 * Free a string conversion object.
 */
static void
free_sconv_object(struct archive_string_conv *sc)
{
	free(sc->from_charset);
	free(sc->to_charset);
#if HAVE_ICONV
	if (sc->cd != (iconv_t)-1)
		iconv_close(sc->cd);
#endif
	free(sc);
}

#if defined(_WIN32) && !defined(__CYGWIN__)
static unsigned
my_atoi(const char *p)
{
	unsigned cp;

	cp = 0;
	while (*p) {
		if (*p >= '0' && *p <= '9')
			cp = cp * 10 + (*p - '0');
		else
			return (-1);
		p++;
	}
	return (cp);
}

#define CP_UTF16LE	1200
#define CP_UTF16BE	1201

/*
 * Translate Charset name (as used by iconv) into CodePage (as used by Windows)
 * Return -1 if failed.
 *
 * Note: This translation code may be insufficient.
 */
static struct charset {
	const char *name;
	unsigned cp;
} charsets[] = {
	/* MUST BE SORTED! */
	{"ASCII", 1252},
	{"ASMO-708", 708},
	{"BIG5", 950},
	{"CHINESE", 936},
	{"CP367", 1252},
	{"CP819", 1252},
	{"CP1025", 21025},
	{"DOS-720", 720},
	{"DOS-862", 862},
	{"EUC-CN", 51936},
	{"EUC-JP", 51932},
	{"EUC-KR", 949},
	{"EUCCN", 51936},
	{"EUCJP", 51932},
	{"EUCKR", 949},
	{"GB18030", 54936},
	{"GB2312", 936},
	{"HEBREW", 1255},
	{"HZ-GB-2312", 52936},
	{"IBM273", 20273},
	{"IBM277", 20277},
	{"IBM278", 20278},
	{"IBM280", 20280},
	{"IBM284", 20284},
	{"IBM285", 20285},
	{"IBM290", 20290},
	{"IBM297", 20297},
	{"IBM367", 1252},
	{"IBM420", 20420},
	{"IBM423", 20423},
	{"IBM424", 20424},
	{"IBM819", 1252},
	{"IBM871", 20871},
	{"IBM880", 20880},
	{"IBM905", 20905},
	{"IBM924", 20924},
	{"ISO-8859-1", 28591},
	{"ISO-8859-13", 28603},
	{"ISO-8859-15", 28605},
	{"ISO-8859-2", 28592},
	{"ISO-8859-3", 28593},
	{"ISO-8859-4", 28594},
	{"ISO-8859-5", 28595},
	{"ISO-8859-6", 28596},
	{"ISO-8859-7", 28597},
	{"ISO-8859-8", 28598},
	{"ISO-8859-9", 28599},
	{"ISO8859-1", 28591},
	{"ISO8859-13", 28603},
	{"ISO8859-15", 28605},
	{"ISO8859-2", 28592},
	{"ISO8859-3", 28593},
	{"ISO8859-4", 28594},
	{"ISO8859-5", 28595},
	{"ISO8859-6", 28596},
	{"ISO8859-7", 28597},
	{"ISO8859-8", 28598},
	{"ISO8859-9", 28599},
	{"JOHAB", 1361},
	{"KOI8-R", 20866},
	{"KOI8-U", 21866},
	{"KS_C_5601-1987", 949},
	{"LATIN1", 1252},
	{"LATIN2", 28592},
	{"MACINTOSH", 10000},
	{"SHIFT-JIS", 932},
	{"SHIFT_JIS", 932},
	{"SJIS", 932},
	{"US", 1252},
	{"US-ASCII", 1252},
	{"UTF-16", 1200},
	{"UTF-16BE", 1201},
	{"UTF-16LE", 1200},
	{"UTF-8", CP_UTF8},
	{"X-EUROPA", 29001},
	{"X-MAC-ARABIC", 10004},
	{"X-MAC-CE", 10029},
	{"X-MAC-CHINESEIMP", 10008},
	{"X-MAC-CHINESETRAD", 10002},
	{"X-MAC-CROATIAN", 10082},
	{"X-MAC-CYRILLIC", 10007},
	{"X-MAC-GREEK", 10006},
	{"X-MAC-HEBREW", 10005},
	{"X-MAC-ICELANDIC", 10079},
	{"X-MAC-JAPANESE", 10001},
	{"X-MAC-KOREAN", 10003},
	{"X-MAC-ROMANIAN", 10010},
	{"X-MAC-THAI", 10021},
	{"X-MAC-TURKISH", 10081},
	{"X-MAC-UKRAINIAN", 10017},
};
static unsigned
make_codepage_from_charset(const char *charset)
{
	char cs[16];
	char *p;
	unsigned cp;
	int a, b;

	if (charset == NULL || strlen(charset) > 15)
		return -1;

	/* Copy name to uppercase. */
	p = cs;
	while (*charset) {
		char c = *charset++;
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		*p++ = c;
	}
	*p++ = '\0';
	cp = -1;

	/* Look it up in the table first, so that we can easily
	 * override CP367, which we map to 1252 instead of 367. */
	a = 0;
	b = sizeof(charsets)/sizeof(charsets[0]);
	while (b > a) {
		int c = (b + a) / 2;
		int r = strcmp(charsets[c].name, cs);
		if (r < 0)
			a = c + 1;
		else if (r > 0)
			b = c;
		else
			return charsets[c].cp;
	}

	/* If it's not in the table, try to parse it. */
	switch (*cs) {
	case 'C':
		if (cs[1] == 'P' && cs[2] >= '0' && cs[2] <= '9') {
			cp = my_atoi(cs + 2);
		} else if (strcmp(cs, "CP_ACP") == 0)
			cp = get_current_codepage();
		else if (strcmp(cs, "CP_OEMCP") == 0)
			cp = get_current_oemcp();
		break;
	case 'I':
		if (cs[1] == 'B' && cs[2] == 'M' &&
		    cs[3] >= '0' && cs[3] <= '9') {
			cp = my_atoi(cs + 3);
		}
		break;
	case 'W':
		if (strncmp(cs, "WINDOWS-", 8) == 0) {
			cp = my_atoi(cs + 8);
			if (cp != 874 && (cp < 1250 || cp > 1258))
				cp = -1;/* This may invalid code. */
		}
		break;
	}
	return (cp);
}

/*
 * Return ANSI Code Page of current locale set by setlocale().
 */
static unsigned
get_current_codepage()
{
	char *locale, *p;
	unsigned cp;

	locale = setlocale(LC_CTYPE, NULL);
	if (locale == NULL)
		return (GetACP());
	if (locale[0] == 'C' && locale[1] == '\0')
		return (CP_C_LOCALE);
	p = strrchr(locale, '.');
	if (p == NULL)
		return (GetACP());
	cp = my_atoi(p+1);
	if (cp <= 0)
		return (GetACP());
	return (cp);
}

/*
 * Translation table between Locale Name and ACP/OEMCP.
 */
static struct {
	unsigned acp;
	unsigned ocp;
	const char *locale;
} acp_ocp_map[] = {
	{  950,  950, "Chinese_Taiwan" },
	{  936,  936, "Chinese_People's Republic of China" },
	{  950,  950, "Chinese_Taiwan" },
	{ 1250,  852, "Czech_Czech Republic" },
	{ 1252,  850, "Danish_Denmark" },
	{ 1252,  850, "Dutch_Netherlands" },
	{ 1252,  850, "Dutch_Belgium" },
	{ 1252,  437, "English_United States" },
	{ 1252,  850, "English_Australia" },
	{ 1252,  850, "English_Canada" },
	{ 1252,  850, "English_New Zealand" },
	{ 1252,  850, "English_United Kingdom" },
	{ 1252,  437, "English_United States" },
	{ 1252,  850, "Finnish_Finland" },
	{ 1252,  850, "French_France" },
	{ 1252,  850, "French_Belgium" },
	{ 1252,  850, "French_Canada" },
	{ 1252,  850, "French_Switzerland" },
	{ 1252,  850, "German_Germany" },
	{ 1252,  850, "German_Austria" },
	{ 1252,  850, "German_Switzerland" },
	{ 1253,  737, "Greek_Greece" },
	{ 1250,  852, "Hungarian_Hungary" },
	{ 1252,  850, "Icelandic_Iceland" },
	{ 1252,  850, "Italian_Italy" },
	{ 1252,  850, "Italian_Switzerland" },
	{  932,  932, "Japanese_Japan" },
	{  949,  949, "Korean_Korea" },
	{ 1252,  850, "Norwegian (BokmOl)_Norway" },
	{ 1252,  850, "Norwegian (BokmOl)_Norway" },
	{ 1252,  850, "Norwegian-Nynorsk_Norway" },
	{ 1250,  852, "Polish_Poland" },
	{ 1252,  850, "Portuguese_Portugal" },
	{ 1252,  850, "Portuguese_Brazil" },
	{ 1251,  866, "Russian_Russia" },
	{ 1250,  852, "Slovak_Slovakia" },
	{ 1252,  850, "Spanish_Spain" },
	{ 1252,  850, "Spanish_Mexico" },
	{ 1252,  850, "Spanish_Spain" },
	{ 1252,  850, "Swedish_Sweden" },
	{ 1254,  857, "Turkish_Turkey" },
	{ 0, 0, NULL}
};

/*
 * Return OEM Code Page of current locale set by setlocale().
 */
static unsigned
get_current_oemcp()
{
	int i;
	char *locale, *p;
	size_t len;

	locale = setlocale(LC_CTYPE, NULL);
	if (locale == NULL)
		return (GetOEMCP());
	if (locale[0] == 'C' && locale[1] == '\0')
		return (CP_C_LOCALE);

	p = strrchr(locale, '.');
	if (p == NULL)
		return (GetOEMCP());
	len = p - locale;
	for (i = 0; acp_ocp_map[i].acp; i++) {
		if (strncmp(acp_ocp_map[i].locale, locale, len) == 0)
			return (acp_ocp_map[i].ocp);
	}
	return (GetOEMCP());
}
#else

/*
 * POSIX platform does not use CodePage.
 */

static unsigned
get_current_codepage()
{
	return (-1);/* Unknown */
}
static unsigned
make_codepage_from_charset(const char *charset)
{
	(void)charset; /* UNUSED */
	return (-1);/* Unknown */
}
static unsigned
get_current_oemcp()
{
	return (-1);/* Unknown */
}

#endif /* defined(_WIN32) && !defined(__CYGWIN__) */

/*
 * Return a string conversion object.
 */
static struct archive_string_conv *
get_sconv_object(struct archive *a, const char *fc, const char *tc, int flag)
{
	struct archive_string_conv *sc;
	unsigned current_codepage;

	sc = find_sconv_object(a, fc, tc);
	if (sc != NULL)
		return (sc);

	if (a == NULL)
		current_codepage = get_current_codepage();
	else
		current_codepage = a->current_codepage;
	sc = create_sconv_object(fc, tc, current_codepage, flag);
	if (sc == NULL) {
		archive_set_error(a, ENOMEM,
		    "Could not allocate memory for a string conversion object");
		return (NULL);
	}

	/* We have to specially treat a string conversion so that
	 * we can correctly translate the wrong format UTF-8 string. */
	if (sc->flag & SCONV_UTF8_LIBARCHIVE_2) {
		if (a != NULL)
			add_sconv_object(a, sc);
		return (sc);
	}
#if HAVE_ICONV
	if (sc->cd == (iconv_t)-1 && (flag & SCONV_BEST_EFFORT) == 0) {
		free_sconv_object(sc);
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "iconv_open failed : Cannot convert "
		    "string to %s", tc);
		return (NULL);
	} else if (a != NULL)
		add_sconv_object(a, sc);
#else /* HAVE_ICONV */
#if defined(_WIN32) && !defined(__CYGWIN__)
	/*
	 * Windows platform can convert a string in current locale from/to
	 * UTF-8 and UTF-16BE.
	 */
	if (sc->flag & (SCONV_UTF16BE | SCONV_WIN_CP)) {
		if (a != NULL)
			add_sconv_object(a, sc);
		return (sc);
	}
#endif /* _WIN32 && !__CYGWIN__ */
	if (!sc->same && (flag & SCONV_BEST_EFFORT) == 0) {
		free_sconv_object(sc);
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "A character-set conversion not fully supported "
		    "on this platform");
		return (NULL);
	} else if (a != NULL)
		add_sconv_object(a, sc);
#endif /* HAVE_ICONV */

	return (sc);
}

static const char *
get_current_charset(struct archive *a)
{
	const char *cur_charset;

	if (a == NULL)
		cur_charset = default_iconv_charset("");
	else {
		cur_charset = default_iconv_charset(a->current_code);
		if (a->current_code == NULL) {
			a->current_code = strdup(cur_charset);
			a->current_codepage = get_current_codepage();
			a->current_oemcp = get_current_oemcp();
		}
	}
	return (cur_charset);
}

/*
 * Make and Return a string conversion object.
 * Return NULL if the platform does not support the specified conversion
 * and best_effort is 0.
 * If best_effort is set, A string conversion object must be returned
 * unless memory allocation for the object fails, but the conversion
 * might fail when non-ASCII code is found.
 */
struct archive_string_conv *
archive_string_conversion_to_charset(struct archive *a, const char *charset,
    int best_effort)
{
	int flag = SCONV_TO_CHARSET;

	if (best_effort)
		flag |= SCONV_BEST_EFFORT;
	return (get_sconv_object(a, get_current_charset(a), charset, flag));
}

struct archive_string_conv *
archive_string_conversion_from_charset(struct archive *a, const char *charset,
    int best_effort)
{
	int flag = SCONV_FROM_CHARSET;

	if (best_effort)
		flag |= SCONV_BEST_EFFORT;
	return (get_sconv_object(a, charset, get_current_charset(a), flag));
}

/*
 * archive_string_default_conversion_*_archive() are provided for Windows
 * platform because other archiver application use CP_OEMCP for
 * MultiByteToWideChar() and WideCharToMultiByte() for the filenames
 * in tar or zip files. But mbstowcs/wcstombs(CRT) usually use CP_ACP
 * unless you use setlocale(LC_ALL, ".OCP")(specify CP_OEMCP).
 * So we should make a string conversion between CP_ACP and CP_OEMCP
 * for compatibillty.
 */
#if defined(_WIN32) && !defined(__CYGWIN__)
struct archive_string_conv *
archive_string_default_conversion_for_read(struct archive *a)
{
	const char *cur_charset = get_current_charset(a);
	char oemcp[16];

	/* NOTE: a check of cur_charset is unneeded but we need
	 * that get_current_charset() has been surely called at
	 * this time whatever C compiler optimized. */
	if (cur_charset != NULL &&
	    (a->current_codepage == CP_C_LOCALE ||
	     a->current_codepage == a->current_oemcp))
		return (NULL);/* no conversion. */

	_snprintf(oemcp, sizeof(oemcp)-1, "CP%d", a->current_oemcp);
	/* Make sure a null termination must be set. */
	oemcp[sizeof(oemcp)-1] = '\0';
	return (get_sconv_object(a, oemcp, cur_charset,
	    SCONV_FROM_CHARSET));
}

struct archive_string_conv *
archive_string_default_conversion_for_write(struct archive *a)
{
	const char *cur_charset = get_current_charset(a);
	char oemcp[16];

	/* NOTE: a check of cur_charset is unneeded but we need
	 * that get_current_charset() has been surely called at
	 * this time whatever C compiler optimized. */
	if (cur_charset != NULL &&
	    (a->current_codepage == CP_C_LOCALE ||
	     a->current_codepage == a->current_oemcp))
		return (NULL);/* no conversion. */

	_snprintf(oemcp, sizeof(oemcp)-1, "CP%d", a->current_oemcp);
	/* Make sure a null termination must be set. */
	oemcp[sizeof(oemcp)-1] = '\0';
	return (get_sconv_object(a, cur_charset, oemcp,
	    SCONV_TO_CHARSET));
}
#else
struct archive_string_conv *
archive_string_default_conversion_for_read(struct archive *a)
{
	(void)a; /* UNUSED */
	return (NULL);
}

struct archive_string_conv *
archive_string_default_conversion_for_write(struct archive *a)
{
	(void)a; /* UNUSED */
	return (NULL);
}
#endif

/*
 * Dispose of all character conversion objects in the archive object.
 */
void
archive_string_conversion_free(struct archive *a)
{
	struct archive_string_conv *sc; 
	struct archive_string_conv *sc_next; 

	for (sc = a->sconv; sc != NULL; sc = sc_next) {
		sc_next = sc->next;
		free_sconv_object(sc);
	}
	a->sconv = NULL;
	free(a->current_code);
	a->current_code = NULL;
}

/*
 * Return a conversion charset name.
 */
const char *
archive_string_conversion_charset_name(struct archive_string_conv *sc)
{
	if (sc->flag & SCONV_TO_CHARSET)
		return (sc->to_charset);
	else
		return (sc->from_charset);
}

/*
 *
 * Copy one archive_string to another in locale conversion.
 *
 *	archive_strncpy_in_locale();
 *	archive_strcpy_in_locale();
 *
 */

static size_t
la_strnlen(const void *_p, size_t n)
{
	size_t s;
	const char *p, *pp;

	if (_p == NULL)
		return (0);
	p = (const char *)_p;

	/* Like strlen(p), except won't examine positions beyond p[n]. */
	s = 0;
	pp = p;
	while (s < n && *pp) {
		pp++;
		s++;
	}
	return (s);
}

int
archive_strncpy_in_locale(struct archive_string *as, const void *_p, size_t n,
    struct archive_string_conv *sc)
{
	as->length = 0;
	if (sc != NULL && (sc->flag & SCONV_UTF16BE)) {
		if (sc->flag & SCONV_TO_CHARSET)
			return (strncpy_to_utf16be(as, _p, n, sc));
		else
			return (strncpy_from_utf16be(as, _p, n, sc));
	}
	return (archive_strncat_in_locale(as, _p, n, sc));
}


#if HAVE_ICONV

/*
 * Return -1 if conversion failes.
 */
int
archive_strncat_in_locale(struct archive_string *as, const void *_p, size_t n,
    struct archive_string_conv *sc)
{
	ICONV_CONST char *inp;
	size_t remaining;
	iconv_t cd;
	const char *src = _p;
	char *outp;
	size_t avail, length;
	int return_value = 0; /* success */

	length = la_strnlen(_p, n);
	/* If sc is NULL, we just make a copy without conversion. */
	if (sc == NULL) {
		archive_string_append(as, src, length);
		return (0);
	}

	/* Perform special sequence for the incorrect UTF-8 made by
	 * libarchive2.x. */
	if (sc->flag & SCONV_UTF8_LIBARCHIVE_2)
		return (strncat_from_utf8_libarchive2(as, _p, length));

	/*
	 * Copy UTF-8 string with a check of CESU-8.
	 * Apparently, iconv does not check surrogate pairs in UTF-8
	 * when both from-charset and to-charset are UTF-8.
	 */
	if (sc != NULL && (sc->flag & SCONV_COPY_UTF8_TO_UTF8) != 0)
		return (strncat_from_utf8_utf8(as, _p, length));

	archive_string_ensure(as, as->length + length*2+1);

	cd = sc->cd;
	if (cd == (iconv_t)-1)
		return (best_effort_strncat_in_locale(as, _p, n, sc));

	inp = (char *)(uintptr_t)src;
	remaining = length;
	outp = as->s + as->length;
	avail = as->buffer_length -1;
	while (remaining > 0) {
		size_t result = iconv(cd, &inp, &remaining, &outp, &avail);

		if (result != (size_t)-1) {
			*outp = '\0';
			as->length = outp - as->s;
			break; /* Conversion completed. */
		} else if (errno == EILSEQ || errno == EINVAL) {
			/* Skip the illegal input bytes. */
			*outp++ = '?';
			avail--;
			inp++;
			remaining--;
			return_value = -1; /* failure */
		} else {
			/* E2BIG no output buffer,
			 * Increase an output buffer.  */
			as->length = outp - as->s;
			archive_string_ensure(as, as->buffer_length * 2);
			outp = as->s + as->length;
			avail = as->buffer_length - as->length -1;
		}
	}
	return (return_value);
}

#else /* HAVE_ICONV */

/*
 * Basically returns -1 because we cannot make a conversion of charset
 * without iconv. Returns 0 if sc is NULL.
 */
int
archive_strncat_in_locale(struct archive_string *as, const void *_p, size_t n,
    struct archive_string_conv *sc)
{
	return (best_effort_strncat_in_locale(as, _p, n, sc));
}

#endif /* HAVE_ICONV */


#if defined(_WIN32) && !defined(__CYGWIN__)

/*
 * Translate a string from a some CodePage to an another CodePage by
 * Windows APIs, and copy the result. Return -1 if conversion failes.
 */
static int
strncat_in_codepage(struct archive_string *as,
    const char *s, size_t length, struct archive_string_conv *sc)
{
	int count, wslen;
	wchar_t *ws;
	BOOL defchar, *dp;
	UINT from_cp, to_cp;

	if (s == NULL || length == 0) {
		/* We must allocate memory even if there is no data.
		 * It simulates archive_string_append behavior. */
		if (archive_string_ensure(as, as->length + 1) == NULL)
			__archive_errx(1, "Out of memory");
		as->s[as->length] = 0;
		return (0);
	}

	from_cp = sc->from_cp;
	to_cp = sc->to_cp;

	if (from_cp == CP_C_LOCALE &&
		(sc->flag & SCONV_TO_CHARSET) != 0) {
		/*
		 * "C" locale special process.
		 */
		wchar_t *wp;
		const unsigned char *mp;

		wp = ws = malloc(sizeof(*ws) * (length+1));
		if (ws == NULL)
			__archive_errx(0, "No memory");

		mp = (const unsigned char *)s;
		count = 0;
		while (count < (int)length && *mp) {
			*wp++ = (wchar_t)*mp++;
			count++;
		}
	} else {
		DWORD mbflag;

		if (sc->flag & SCONV_FROM_CHARSET)
			mbflag = 0;
		else
			mbflag = MB_PRECOMPOSED;

		count = MultiByteToWideChar(from_cp,
		    mbflag, s, length, NULL, 0);
		if (count == 0) {
			archive_string_append(as, s, length);
			return (-1);
		}
		ws = malloc(sizeof(*ws) * (count+1));
		if (ws == NULL)
			__archive_errx(0, "No memory");
		count = MultiByteToWideChar(from_cp,
		    mbflag, s, length, ws, count);
	}
	ws[count] = L'\0';
	wslen = count;

	if (to_cp == CP_C_LOCALE &&
		(sc->flag & SCONV_FROM_CHARSET) != 0) {
		/*
		 * "C" locale special process.
		 */
		char *p;
		wchar_t *wp;

		archive_string_ensure(as, as->length + wslen +1);
		p = as->s + as->length;
		wp = ws;
		count = 0;
		defchar = 0;
		while (count < wslen && *wp) {
			if (*wp > 255) {
				*p++ = '?';
				wp++;
				defchar = 1;
			} else
				*p++ = (char)*wp++;
			count++;
		}
	} else {
		count = WideCharToMultiByte(to_cp, 0, ws, wslen,
		    NULL, 0, NULL, NULL);
		if (count == 0) {
			free(ws);
			archive_string_append(as, s, length);
			return (-1);
		}
		defchar = 0;
		if (to_cp == CP_UTF8)
			dp = NULL;
		else
			dp = &defchar;
		archive_string_ensure(as, as->length + count +1);
		count = WideCharToMultiByte(to_cp, 0, ws, wslen,
		    as->s + as->length, count, NULL, dp);
	}
	as->length += count;
	as->s[as->length] = '\0';
	free(ws);
	return (defchar?-1:0);
}

/*
 * Test whether MBS ==> WCS is okay.
 */
static int
invalid_mbs(const void *_p, size_t n, struct archive_string_conv *sc)
{
	const char *p = (const char *)_p;
	unsigned codepage;
	DWORD mbflag = MB_ERR_INVALID_CHARS;

	if (sc->flag & SCONV_FROM_CHARSET)
		codepage = sc->to_cp;
	else
		codepage = sc->from_cp;

	if (codepage == CP_C_LOCALE)
		return (0);
	if (codepage != CP_UTF8)
		mbflag |= MB_PRECOMPOSED;

	if (MultiByteToWideChar(codepage, mbflag, p, n, NULL, 0) == 0)
		return (-1); /* Invalid */
	return (0); /* Okay */
}

#else

/*
 * Test whether MBS ==> WCS is okay.
 */
static int
invalid_mbs(const void *_p, size_t n, struct archive_string_conv *sc)
{
	const char *p = (const char *)_p;
	size_t r;

	(void)sc; /* UNUSED */
#if HAVE_MBRTOWC
	mbstate_t shift_state;

	memset(&shift_state, 0, sizeof(shift_state));
#else
	/* Clear the shift state before starting. */
	mbtowc(NULL, NULL, 0);
#endif
	while (n) {
		wchar_t wc;

#if HAVE_MBRTOWC
		r = mbrtowc(&wc, p, n, &shift_state);
#else
		r = mbtowc(&wc, p, n);
#endif
		if (r == (size_t)-1 || r == (size_t)-2)
			return (-1);/* Invalid. */
		if (r == 0)
			break;
		p += r;
		n -= r;
	}
	return (0); /* All Okey. */
}

#endif /* defined(_WIN32) && !defined(__CYGWIN__) */

/*
 * Test that MBS consists of ASCII code only.
 */
static int
is_all_ascii_code(struct archive_string *as)
{
	size_t i;

	for (i = 0; i < as->length; i++)
		if (((unsigned char)as->s[i]) > 0x7f)
			return (0);
	/* It seems the string we have checked is all ASCII code. */
	return (1);
}

/*
 * Basically returns -1 because we cannot make a conversion of charset.
 * Returns 0 if sc is NULL.
 */
static int
best_effort_strncat_in_locale(struct archive_string *as, const void *_p,
    size_t n, struct archive_string_conv *sc)
{
	size_t length = la_strnlen(_p, n);

#if defined(_WIN32) && !defined(__CYGWIN__)
	if (sc != NULL && (sc->flag & SCONV_WIN_CP) != 0)
		return (strncat_in_codepage(as, _p, length, sc));
#endif
	/* Perform special sequence for the incorrect UTF-8 made by
	 * libarchive2.x. */
	if (sc != NULL && (sc->flag & SCONV_UTF8_LIBARCHIVE_2) != 0)
		return (strncat_from_utf8_libarchive2(as, _p, length));

	/* Copy UTF-8 string with a check of CESU-8. */
	if (sc != NULL && (sc->flag & SCONV_COPY_UTF8_TO_UTF8) != 0)
		return (strncat_from_utf8_utf8(as, _p, length));

	archive_string_append(as, _p, length);
	/* If charset is NULL, just make a copy, so return 0 as success. */
	if (sc == NULL || (sc->same && invalid_mbs(_p, n, sc) == 0))
		return (0);
	if (is_all_ascii_code(as))
		return (0);
	return (-1);
}


/*
 * Unicode conversion functions.
 *   - UTF-8 <===> UTF-8 in removing surrogate pairs.
 *   - UTF-8 made by libarchive 2.x ===> UTF-8.
 *   - UTF-16BE <===> UTF-8.
 *
 */
#define IS_HIGH_SURROGATE(uc)	((uc) >= 0xD800 && (uc) <= 0xDBFF)
#define IS_LOW_SURROGATE(uc)	((uc) >= 0xDC00 && (uc) <= 0xDFFF)
#define IS_SURROGATE(uc)	((uc) >= 0xD800 && (uc) <= 0xDFFF)
#define UNICODE_MAX		0x10FFFF

/*
 * Utility to convert a single UTF-8 sequence.
 */
static int
_utf8_to_unicode(uint32_t *pwc, const char *s, size_t n)
{
	static unsigned char utf8_count[256] = {
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,/* 00 - 0F */
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,/* 10 - 1F */
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,/* 20 - 2F */
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,/* 30 - 3F */
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,/* 40 - 4F */
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,/* 50 - 5F */
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,/* 60 - 6F */
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,/* 70 - 7F */
		 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,/* 80 - 8F */
		 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,/* 90 - 9F */
		 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,/* A0 - AF */
		 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,/* B0 - BF */
		 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,/* C0 - CF */
		 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,/* D0 - DF */
		 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,/* E0 - EF */
		 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 /* F0 - FF */
	};
	int ch;
	unsigned char cnt;
	uint32_t wc;

	/* Sanity check. */
	if (n == 0)
		return (0);
	/*
	 * Decode 1-4 bytes depending on the value of the first byte.
	 */
	ch = (unsigned char)*s;
	if (ch == 0)
		return (0); /* Standard:  return 0 for end-of-string. */
	cnt = utf8_count[ch];

	/* Invalide sequence or there are not plenty bytes. */
	if (n < cnt)
		return (-1);

	/* Make a Unicode code point from a single UTF-8 sequence. */
	switch (cnt) {
	case 1:	/* 1 byte sequence. */
		*pwc = ch & 0x7f;
		return (cnt);
	case 2:	/* 2 bytes sequence. */
		if ((s[1] & 0xc0) != 0x80) return (-1);
		*pwc = ((ch & 0x1f) << 6) | (s[1] & 0x3f);
		return (cnt);
	case 3:	/* 3 bytes sequence. */
		if ((s[1] & 0xc0) != 0x80) return (-1);
		if ((s[2] & 0xc0) != 0x80) return (-1);
		wc = ((ch & 0x0f) << 12)
		    | ((s[1] & 0x3f) << 6)
		    | (s[2] & 0x3f);
		if (wc < 0x800)
			return (-1);/* Overlong sequence. */
		break;
	case 4:	/* 4 bytes sequence. */
		if (n < 4)
			return (-1);
		if ((s[1] & 0xc0) != 0x80) return (-1);
		if ((s[2] & 0xc0) != 0x80) return (-1);
		if ((s[3] & 0xc0) != 0x80) return (-1);
		wc = ((ch & 0x07) << 18)
		    | ((s[1] & 0x3f) << 12)
		    | ((s[2] & 0x3f) << 6)
		    | (s[3] & 0x3f);
		if (wc < 0x10000)
			return (-1);/* Overlong sequence. */
		break;
	default:
		return (-1);
	}

	/* The code point larger than 0x10FFFF is not leagal
	 * Unicode values. */
	if (wc > UNICODE_MAX)
		return (-1);
	/* Correctly gets a Unicode, returns used bytes. */
	*pwc = wc;
	return (cnt);
}

static int
utf8_to_unicode(uint32_t *pwc, const char *s, size_t n)
{
	uint32_t wc;
	int cnt;

	cnt = _utf8_to_unicode(&wc, s, n);
	/* Surrogate is not leagal Unicode values. */
	if (cnt == 3 && IS_SURROGATE(wc))
		return (-1);
	*pwc = wc;
	return (cnt);
}

static inline uint32_t
combine_surrogate_pair(uint32_t uc, uint32_t uc2)
{
	uc -= 0xD800;
	uc *= 0x400;
	uc += uc2 - 0xDC00;
	uc += 0x10000;
	return (uc);
}

/*
 * Convert a single UTF-8/CESU-8 sequence to a Unicode code point in
 * removing surrogate pairs.
 *
 * CESU-8: The Compatibility Encoding Scheme for UTF-16.
 */
static int
cesu8_to_unicode(uint32_t *pwc, const char *s, size_t n)
{
	uint32_t wc, wc2;
	int cnt;

	cnt = _utf8_to_unicode(&wc, s, n);
	if (cnt == 3 && IS_HIGH_SURROGATE(wc)) {
		if (n - 3 < 3)
			return (-1);
		cnt = _utf8_to_unicode(&wc2, s+3, n-3);
		if (cnt != 3 || !IS_LOW_SURROGATE(wc2))
			return (-1);
		wc = combine_surrogate_pair(wc, wc2);
		cnt = 6;
	}
	*pwc = wc;
	return (cnt);
}

/*
 * Convert a Unicode code point to a single UTF-8 sequence.
 *
 * NOTE:This function does not check if the Unicode is leagal or not.
 * Please you definitely check it before calling this.
 */
static int
unicode_to_utf8(char *p, uint32_t uc)
{
	char *_p = p;

	/* Translate code point to UTF8 */
	if (uc <= 0x7f) {
		*p++ = (char)uc;
	} else if (uc <= 0x7ff) {
		*p++ = 0xc0 | ((uc >> 6) & 0x1f);
		*p++ = 0x80 | (uc & 0x3f);
	} else if (uc <= 0xffff) {
		*p++ = 0xe0 | ((uc >> 12) & 0x0f);
		*p++ = 0x80 | ((uc >> 6) & 0x3f);
		*p++ = 0x80 | (uc & 0x3f);
	} else {
		*p++ = 0xf0 | ((uc >> 18) & 0x07);
		*p++ = 0x80 | ((uc >> 12) & 0x3f);
		*p++ = 0x80 | ((uc >> 6) & 0x3f);
		*p++ = 0x80 | (uc & 0x3f);
	}
	return ((int)(p - _p));
}

/*
 * Copy UTF-8 string in checking surrogate pair.
 * If any surrogate pair are found, it would be canonicalized.
 */
static int
strncat_from_utf8_utf8(struct archive_string *as, const char *s, size_t len)
{
	char *p;
	int n, ret = 0;

	/*
	 * Additional buffer size will not be larger than a value of 'len'
	 * even if a conversion of CESU-8 happen. Because a surrogate pair
	 * code point of CESU-8 sequence takes six bytes, but a new UTF-8
	 * sequence converted from the CESU-8 takes four bytes.
	 */
	if (archive_string_ensure(as, as->length + len + 1) == NULL)
		__archive_errx(1, "Out of memory");

	p = as->s + as->length;
	do {
		uint32_t uc;
		const char *ss = s;

		while ((n = utf8_to_unicode(&uc, s, len)) > 0) {
			s += n;
			len -= n;
		}
		if (ss < s) {
			memcpy(p, ss, s - ss);
			p += s - ss;
		}

		if (n == -1) {
			/* Is this CESU-8 ? */
			n = cesu8_to_unicode(&uc, s, len);
			if (n == -1) {
				/* Skip current byte. */
				*p++ = '?';
				n = 1;
				ret = -1;
			} else {
				/* Rebuild UTF-8. */
				p += unicode_to_utf8(p, uc);
			}
			s += n;
			len -= n;
		}
	} while (n > 0);
	as->length = p - as->s;
	as->s[as->length] = '\0';
	return (ret);
}

/*
 * libarchive 2.x made incorrect UTF-8 strings in the wrong assumuption
 * that WCS is Unicode. it is true for servel platforms but some are false.
 * And then people who did not use UTF-8 locale on the non Unicode WCS
 * platform and made a tar file with libarchive(mostly bsdtar) 2.x. Those
 * now cannot get right filename from libarchive 3.x and later since we
 * fixed the wrong assumption and it is incompatible to older its versions.
 * So we provide special option, "utf8type=libarchive2.x", for resolving it.
 * That option enable the string conversion of libarchive 2.x.
 *
 * Translates the wrong UTF-8 string made by libarchive 2.x into current
 * locale character set and appends to the archive_string.
 * Note: returns -1 if conversion fails.
 */
static int
strncat_from_utf8_libarchive2(struct archive_string *as,
    const char *s, size_t len)
{
	int n;
	char *p;
	char *end;
#if HAVE_WCRTOMB
	mbstate_t shift_state;

	memset(&shift_state, 0, sizeof(shift_state));
#else
	/* Clear the shift state before starting. */
	wctomb(NULL, L'\0');
#endif
	/*
	 * Allocate buffer for MBS.
	 * We need this allocation here since it is possible that
	 * as->s is still NULL.
	 */
	if (archive_string_ensure(as, as->length + len + 1) == NULL)
		__archive_errx(1, "Out of memory");

	p = as->s + as->length;
	end = as->s + as->buffer_length - MB_CUR_MAX -1;
	while (*s != '\0' && len > 0) {
		wchar_t wc;
		uint32_t unicode;

		if (p >= end) {
			as->length = p - as->s;
			/* Re-allocate buffer for MBS. */
			if (archive_string_ensure(as,
			    as->length + len * 2 + 1) == NULL)
				__archive_errx(1, "Out of memory");
			p = as->s + as->length;
			end = as->s + as->buffer_length - MB_CUR_MAX -1;
		}

		/*
		 * As libarchie 2.x, translates the wrong UTF-8 MBS into
		 * a wide-character in the assumption that WCS is Unicode.
		 */
		n = utf8_to_unicode(&unicode, s, len);
		if (n == -1)
			return (-1);
		s += n;
		len -= n;

		/*
		 * Translates the wide-character into the current locale MBS.
		 */
		wc = (wchar_t)unicode;
#if HAVE_WCRTOMB
		n = wcrtomb(p, wc, &shift_state);
#else
		n = wctomb(p, wc);
#endif
		if (n == -1)
			return (-1);
		p += n;
	}
	as->length = p - as->s;
	as->s[as->length] = '\0';
	return (0);
}


/*
 * Conversion functions between current locale dependent MBS and UTF-16BE.
 *   strncpy_from_utf16be() : UTF-16BE --> MBS
 *   strncpy_to_utf16be()   : MBS --> UTF16BE
 */
#if HAVE_ICONV

/*
 * Convert a UTF-16BE string to current locale and copy the result.
 * Return -1 if conversion failes.
 */
static int
strncpy_from_utf16be(struct archive_string *as, const void *_p, size_t bytes,
    struct archive_string_conv *sc)
{
	ICONV_CONST char *inp;
	const char *utf16 = (const char *)_p;
	size_t remaining;
	iconv_t cd;
	char *outp;
	size_t avail, outbase;
	int return_value = 0; /* success */

	archive_string_empty(as);

	bytes &= ~1;
	archive_string_ensure(as, bytes+1);

	cd = sc->cd;
	inp = (char *)(uintptr_t)utf16;
	remaining = bytes;
	outp = as->s;
	avail = outbase = bytes;
	while (remaining > 0) {
		size_t result = iconv(cd, &inp, &remaining, &outp, &avail);

		if (result != (size_t)-1) {
			*outp = '\0';
			as->length = outbase - avail;
			break; /* Conversion completed. */
		} else if (errno == EILSEQ || errno == EINVAL) {
			/* Skip the illegal input bytes. */
			*outp++ = '?';
			avail --;
			inp += 2;
			remaining -= 2;
			return_value = -1; /* failure */
		} else {
			/* E2BIG no output buffer,
			 * Increase an output buffer.  */
			as->length = outbase - avail;
			outbase *= 2;
			archive_string_ensure(as, outbase+1);
			outp = as->s + as->length;
			avail = outbase - as->length;
		}
	}
	return (return_value);
}

/*
 * Convert a current locale string to UTF-16BE and copy the result.
 * Return -1 if conversion failes.
 */
static int
strncpy_to_utf16be(struct archive_string *a16be, const void *_p,
    size_t length, struct archive_string_conv *sc)
{
	ICONV_CONST char *inp;
	const char *src = (const char *)_p;
	size_t remaining;
	iconv_t cd;
	char *outp;
	size_t avail, outbase;
	int return_value = 0; /* success */

	archive_string_empty(a16be);

	archive_string_ensure(a16be, (length+1)*2);

	cd = sc->cd;
	inp = (char *)(uintptr_t)src;
	remaining = length;
	outp = a16be->s;
	avail = outbase = length * 2;
	while (remaining > 0) {
		size_t result = iconv(cd, &inp, &remaining, &outp, &avail);

		if (result != (size_t)-1) {
			outp[0] = 0; outp[1] = 0;
			a16be->length = outbase - avail;
			break; /* Conversion completed. */
		} else if (errno == EILSEQ || errno == EINVAL) {
			/* Skip the illegal input bytes. */
			*outp++ = 0; *outp++ = '?';
			avail -= 2;
			inp ++;
			remaining --;
			return_value = -1; /* failure */
		} else {
			/* E2BIG no output buffer,
			 * Increase an output buffer.  */
			a16be->length = outbase - avail;
			outbase *= 2;
			archive_string_ensure(a16be, outbase+2);
			outp = a16be->s + a16be->length;
			avail = outbase - a16be->length;
		}
	}
	return (return_value);
}

#elif defined(_WIN32) && !defined(__CYGWIN__)

/*
 * Convert a UTF-16BE string to current locale and copy the result.
 * Return -1 if conversion failes.
 */
static int
strncpy_from_utf16be(struct archive_string *as, const void *_p, size_t bytes,
    struct archive_string_conv *sc)
{
	const char *utf16 = (const char *)_p;
	int ll;
	BOOL defchar;
	char *mbs;
	size_t mbs_size;
	int ret = 0;

	archive_string_empty(as);
	bytes &= ~1;
	archive_string_ensure(as, bytes+1);
	mbs = as->s;
	mbs_size = as->buffer_length-1;
	while (bytes) {
		uint16_t val = archive_be16dec(utf16);
		ll = WideCharToMultiByte(sc->to_cp, 0,
		    (LPCWSTR)&val, 1, mbs, mbs_size,
			NULL, &defchar);
		if (ll == 0) {
			*mbs = '\0';
			return (-1);
		} else if (defchar)
			ret = -1;
		as->length += ll;
		mbs += ll;
		mbs_size -= ll;
		bytes -= 2;
		utf16 += 2;
	}
	*mbs = '\0';
	return (ret);
}

static int
is_big_endian()
{
	uint16_t d = 1;

	return (archive_be16dec(&d) == 1);
}

/*
 * Convert a current locale string to UTF-16BE and copy the result.
 * Return -1 if conversion failes.
 */
static int
strncpy_to_utf16be(struct archive_string *a16be, const void *_p, size_t length,
    struct archive_string_conv *sc)
{
	const char *s = (const char *)_p;
	size_t count;

	archive_string_ensure(a16be, (length + 1) * 2);
	archive_string_empty(a16be);
	do {
		count = MultiByteToWideChar(sc->from_cp,
		    MB_PRECOMPOSED, s, length,
		    (LPWSTR)a16be->s, (int)a16be->buffer_length - 2);
		if (count == 0 &&
		    GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
			/* Need more buffer for UTF-16 string */
			count = MultiByteToWideChar(sc->from_cp,
			    MB_PRECOMPOSED, s, length, NULL, 0);
			archive_string_ensure(a16be, (count +1) * 2);
			continue;
		}
		if (count == 0)
			return (-1);
	} while (0);
	a16be->length = count * 2;
	a16be->s[a16be->length] = 0;
	a16be->s[a16be->length+1] = 0;

	if (!is_big_endian()) {
		char *s = a16be->s;
		size_t l = a16be->length;
		while (l > 0) {
			uint16_t v = archive_le16dec(s);
			archive_be16enc(s, v);
			s += 2;
			l -= 2;
		}
	}
	return (0);
}

#else

/*
 * In case the platform does not have iconv nor other character-set
 * conversion functions, We cannot handle UTF-16BE character-set,
 * but there is a chance if a string consists just ASCII code or
 * a current locale is UTF-8.
 *
 */

/*
 * UTF-16BE to UTF-8.
 * Note: returns non-zero if conversion fails, but still leaves a best-effort
 * conversion in the argument as.
 */
static int
string_append_from_utf16be_to_utf8(struct archive_string *as,
    const char *utf16be, size_t bytes)
{
	char *p, *end;
	unsigned uc;
	size_t base_size;
	int return_val = 0; /* success */

	bytes &= ~1;
	archive_string_ensure(as, bytes+1);
	base_size = as->buffer_length;
	p = as->s + as->length;
	end = as->s + as->buffer_length -1;
	while (bytes >= 2) {
		/* Expand the buffer when we have <4 bytes free. */
		if (end - p < 4) {
			size_t l = p - as->s;
			base_size *= 2;
			archive_string_ensure(as, base_size);
			p = as->s + l;
			end = as->s + as->buffer_length -1;
		}

		uc = archive_be16dec(utf16be);
		utf16be += 2; bytes -=2;
		
		/* If this is a surrogate pair, assemble the full code point.*/
		if (IS_HIGH_SURROGATE(uc)) {
			unsigned uc2;

			if (bytes >= 2)
				uc2 = archive_be16dec(utf16be);
			else
				uc2 = 0;
			if (IS_LOW_SURROGATE(uc2)) {
				uc = combine_surrogate_pair(uc, uc2);
				utf16be += 2; bytes -=2;
			} else {
				/* Wrong sequence. */
				*p++ = '?';
				return_val = -1;
				break;
			}
		}

		/*
		 * Surrogate pair values(0xd800 through 0xdfff) are only
		 * used by UTF-16, so, after above culculation, the code
		 * must not be surrogate values, and Unicode has no codes
		 * larger than 0x10ffff. Thus, those are not leagal Unicode
		 * values.
		 */
		if (IS_SURROGATE(uc) || uc > UNICODE_MAX) {
			*p++ = '?';
			return_val = -1;
			break;
		}

		/* Translate code point to UTF8 */
		p += unicode_to_utf8(p, uc);
	}
	as->length = p - as->s;
	*p = '\0';
	return (return_val);
}

/*
 * Return a UTF-16BE string by converting this archive_string from UTF-8.
 * Returns 0 on success, non-zero if conversion fails.
 */
static int
string_append_from_utf8_to_utf16be(struct archive_string *as,
    const char *p, size_t len)
{
	char *s, *end;
	size_t base_size;
	uint32_t wc;/* Must be large enough for a 21-bit Unicode code point. */
	int n;
	int return_val = 0; /* success */

	archive_string_ensure(as, (len+1)*2);
	base_size = as->buffer_length;
	s = as->s + as->length;
	end = as->s + as->buffer_length -2;
	while (len > 0) {
		/* Expand the buffer when we have <4 bytes free. */
		if (end - s < 4) {
			size_t l = p - as->s;
			base_size *= 2;
			archive_string_ensure(as, base_size);
			s = as->s + l;
			end = as->s + as->buffer_length -2;
		}
		n = utf8_to_unicode(&wc, p, len);
		if (n == 0)
			break;
		if (n < 0) {
			return (-1);
		}
		p += n;
		len -= n;

		if (wc > 0xffff) {
			/* We have a code point that won't fit into a
			 * wchar_t; convert it to a surrogate pair. */
			wc -= 0x10000;
			archive_be16enc(s, ((wc >> 10) & 0x3ff) + 0xD800);
			archive_be16enc(s+2, (wc & 0x3ff) + 0xDC00);
			s += 4;
		} else {
			archive_be16enc(s, wc);
			s += 2;
		}
	}
	as->length = s - as->s;
	*s++ = 0; *s = 0;
	return (return_val);
}

/*
 * Convert a UTF-16BE string to current locale and copy the result.
 * Return -1 if conversion failes.
 */
static int
strncpy_from_utf16be(struct archive_string *as, const void *_p, size_t bytes,
    struct archive_string_conv *sc)
{
	const char *utf16 = (const char *)_p;
	char *mbs;
	int ret;

	archive_string_empty(as);

	/*
	 * If the current locale is UTF-8, we can translate a UTF-16BE
	 * string into a UTF-8 string.
	 */
	if (strcmp(sc->to_charset, "UTF-8") == 0)
		return (string_append_from_utf16be_to_utf8(as, utf16, bytes));

	/*
	 * Other case, we should do the best effort.
	 * If all character are ASCII(<0x7f), we can convert it.
	 * if not , we set a alternative character and return -1.
	 */
	ret = 0;
	bytes &= ~1;
	archive_string_ensure(as, bytes+1);
	mbs = as->s;
	while (bytes) {
		uint16_t val = archive_be16dec(utf16);
		if (val >= 0x80) {
			/* We cannot handle it. */
			*mbs++ = '?';
			ret =  -1;
		} else
			*mbs++ = (char)val;
		as->length ++;
		bytes -= 2;
		utf16 += 2;
	}
	*mbs = '\0';
	return (ret);
}

/*
 * Convert a current locale string to UTF-16BE and copy the result.
 * Return -1 if conversion failes.
 */
static int
strncpy_to_utf16be(struct archive_string *a16be, const void *_p, size_t length,
    struct archive_string_conv *sc)
{
	const char *s = (const char *)_p;
	char *utf16;
	size_t remaining;
	int ret;

	archive_string_empty(a16be);

	/*
	 * If the current locale is UTF-8, we can translate a UTF-8
	 * string into a UTF-16BE string.
	 */
	if (strcmp(sc->from_charset, "UTF-8") == 0)
		return (string_append_from_utf8_to_utf16be(a16be, s, length));

	/*
	 * Other case, we should do the best effort.
	 * If all character are ASCII(<0x7f), we can convert it.
	 * if not , we set a alternative character and return -1.
	 */
	ret = 0;
	remaining = length;
	archive_string_ensure(a16be, (length + 1) * 2);
	utf16 = a16be->s;
	while (remaining--) {
		if (*(unsigned char *)s >= 0x80) {
			/* We cannot handle it. */
			*utf16++ = 0;
			*utf16++ = '?';
			ret = -1;
		} else {
			*utf16++ = 0;
			*utf16++ = *s++;
		}
		a16be->length += 2;
	}
	a16be->s[a16be->length] = 0;
	a16be->s[a16be->length+1] = 0;
	return (ret);
}

#endif


/*
 * Multistring operations.
 */

void
archive_mstring_clean(struct archive_mstring *aes)
{
	archive_wstring_free(&(aes->aes_wcs));
	archive_string_free(&(aes->aes_mbs));
	archive_string_free(&(aes->aes_utf8));
	aes->aes_set = 0;
}

void
archive_mstring_copy(struct archive_mstring *dest, struct archive_mstring *src)
{
	dest->aes_set = src->aes_set;
	archive_string_copy(&(dest->aes_mbs), &(src->aes_mbs));
	archive_string_copy(&(dest->aes_utf8), &(src->aes_utf8));
	archive_wstring_copy(&(dest->aes_wcs), &(src->aes_wcs));
}

const char *
archive_mstring_get_utf8(struct archive *a, struct archive_mstring *aes)
{
	struct archive_string_conv *sc;
	int r;

	/* If we already have a UTF8 form, return that immediately. */
	if (aes->aes_set & AES_SET_UTF8)
		return (aes->aes_utf8.s);

	if (aes->aes_set & AES_SET_MBS) {
		sc = archive_string_conversion_to_charset(a, "UTF-8", 1);
		if (sc == NULL)
			return (NULL);/* Couldn't allocate memory for sc. */
		r = archive_strncpy_in_locale(&(aes->aes_mbs), aes->aes_mbs.s,
		    aes->aes_mbs.length, sc);
		if (a == NULL)
			free_sconv_object(sc);
		if (r == 0) {
			aes->aes_set |= AES_SET_UTF8;
			return (aes->aes_utf8.s);
		}
	}
	return (NULL);
}

const char *
archive_mstring_get_mbs(struct archive *a, struct archive_mstring *aes)
{
	struct archive_string_conv *sc;
	int r;

	/* If we already have an MBS form, return that immediately. */
	if (aes->aes_set & AES_SET_MBS)
		return (aes->aes_mbs.s);
	/* If there's a WCS form, try converting with the native locale. */
	if ((aes->aes_set & AES_SET_WCS)
	    && archive_string_append_from_wcs(&(aes->aes_mbs),
			aes->aes_wcs.s, aes->aes_wcs.length) == 0) {
		aes->aes_set |= AES_SET_MBS;
		return (aes->aes_mbs.s);
	}
	/* If there's a UTF-8 form, try converting with the native locale. */
	if (aes->aes_set & AES_SET_UTF8) {
		sc = archive_string_conversion_from_charset(a, "UTF-8", 1);
		if (sc == NULL)
			return (NULL);/* Couldn't allocate memory for sc. */
		r = archive_strncpy_in_locale(&(aes->aes_mbs),
			aes->aes_utf8.s, aes->aes_utf8.length, sc);
		if (a == NULL)
			free_sconv_object(sc);
		if (r == 0) {
			aes->aes_set |= AES_SET_UTF8;
			return (aes->aes_utf8.s);
		}
	}
	return (NULL);
}

const wchar_t *
archive_mstring_get_wcs(struct archive *a, struct archive_mstring *aes)
{
	(void)a;/* UNUSED */
	/* Return WCS form if we already have it. */
	if (aes->aes_set & AES_SET_WCS)
		return (aes->aes_wcs.s);
	/* Try converting MBS to WCS using native locale. */
	if ((aes->aes_set & AES_SET_MBS)
	    && 0 == archive_wstring_append_from_mbs(&(aes->aes_wcs),
			aes->aes_mbs.s, aes->aes_mbs.length)) {
		aes->aes_set |= AES_SET_WCS;
		return (aes->aes_wcs.s);
	}
	return (NULL);
}

int
archive_mstring_copy_mbs(struct archive_mstring *aes, const char *mbs)
{
	if (mbs == NULL) {
		aes->aes_set = 0;
		return (0);
	}
	return (archive_mstring_copy_mbs_len(aes, mbs, strlen(mbs)));
}

int
archive_mstring_copy_mbs_len(struct archive_mstring *aes, const char *mbs,
    size_t len)
{
	if (mbs == NULL) {
		aes->aes_set = 0;
		return (0);
	}
	aes->aes_set = AES_SET_MBS; /* Only MBS form is set now. */
	archive_strncpy(&(aes->aes_mbs), mbs, len);
	archive_string_empty(&(aes->aes_utf8));
	archive_wstring_empty(&(aes->aes_wcs));
	return (0);
}

int
archive_mstring_copy_wcs(struct archive_mstring *aes, const wchar_t *wcs)
{
	return archive_mstring_copy_wcs_len(aes, wcs, wcs == NULL ? 0 : wcslen(wcs));
}

int
archive_mstring_copy_wcs_len(struct archive_mstring *aes, const wchar_t *wcs,
    size_t len)
{
	if (wcs == NULL) {
		aes->aes_set = 0;
	}
	aes->aes_set = AES_SET_WCS; /* Only WCS form set. */
	archive_string_empty(&(aes->aes_mbs));
	archive_string_empty(&(aes->aes_utf8));
	archive_wstrncpy(&(aes->aes_wcs), wcs, len);
	return (0);
}

/*
 * The 'update' form tries to proactively update all forms of
 * this string (WCS and MBS) and returns an error if any of
 * them fail.  This is used by the 'pax' handler, for instance,
 * to detect and report character-conversion failures early while
 * still allowing clients to get potentially useful values from
 * the more tolerant lazy conversions.  (get_mbs and get_wcs will
 * strive to give the user something useful, so you can get hopefully
 * usable values even if some of the character conversions are failing.)
 */
/* TODO: Reverse the return values here so that zero is success. */
int
archive_mstring_update_utf8(struct archive *a, struct archive_mstring *aes,
    const char *utf8)
{
	struct archive_string_conv *sc;
	int r;

	if (utf8 == NULL) {
		aes->aes_set = 0;
		return (1); /* Succeeded in clearing everything. */
	}

	/* Save the UTF8 string. */
	archive_strcpy(&(aes->aes_utf8), utf8);

	/* Empty the mbs and wcs strings. */
	archive_string_empty(&(aes->aes_mbs));
	archive_wstring_empty(&(aes->aes_wcs));

	aes->aes_set = AES_SET_UTF8;	/* Only UTF8 is set now. */

	/* Try converting UTF-8 to MBS, return false on failure. */
	sc = archive_string_conversion_from_charset(a, "UTF-8", 1);
	if (sc == NULL)
		return (0);/* Couldn't allocate memory for sc. */
	r = archive_strcpy_in_locale(&(aes->aes_mbs), utf8, sc);
	if (a == NULL)
		free_sconv_object(sc);
	if (r != 0)
		return (0);
	aes->aes_set = AES_SET_UTF8 | AES_SET_MBS; /* Both UTF8 and MBS set. */

	/* Try converting MBS to WCS, return false on failure. */
	if (archive_wstring_append_from_mbs(&(aes->aes_wcs), aes->aes_mbs.s,
	    aes->aes_utf8.length))
		return (0);
	aes->aes_set = AES_SET_UTF8 | AES_SET_WCS | AES_SET_MBS;

	/* All conversions succeeded. */
	return (1);
}
