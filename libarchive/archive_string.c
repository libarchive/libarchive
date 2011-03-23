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
#endif

#include "archive_endian.h"
#include "archive_private.h"
#include "archive_string.h"

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
 * Character set conversion functions.
 *
 * Conversions involving MBS use the "current locale" as
 * stored in the archive object passed in.  Conversions
 * between UTF8 and Unicode do not require any such help,
 * of course.
 *
 * TODO: Provide utility functions to set the current locale.
 * Remember that Windows stores character encoding as an int,
 * POSIX typically uses a string.  I'd rather not bother trying
 * to translate between the two.
 */

/*
 * Unicode to UTF-8.
 * Note: returns non-zero if conversion fails, but still leaves a best-effort
 * conversion in the argument as.
 */
int
archive_string_append_from_unicode_to_utf8(struct archive_string *as, const wchar_t *w, size_t len)
{
	char *p;
	unsigned wc;
	char buff[256];
	int return_val = 0; /* success */

	/*
	 * Convert one wide char at a time into 'buff', whenever that
	 * fills, append it to the string.
	 */
	p = buff;
	while (len-- > 0) {
		/* Flush the buffer when we have <=16 bytes free. */
		/* (No encoding has a single character >16 bytes.) */
		if ((size_t)(p - buff) >= (size_t)(sizeof(buff) - 16)) {
			*p = '\0';
			archive_strcat(as, buff);
			p = buff;
		}
		wc = *w++;
		/* If this is a surrogate pair, assemble the full code point.*/
		/* Note: wc must not be wchar_t here, because the full code
		 * point can be more than 16 bits! */
		if (wc >= 0xD800 && wc <= 0xDBff
		    && *w >= 0xDC00 && *w <= 0xDFFF) {
			wc -= 0xD800;
			wc *= 0x400;
			wc += (*w - 0xDC00);
			wc += 0x10000;
			++w;
		}
		/* Translate code point to UTF8 */
		if (wc <= 0x7f) {
			*p++ = (char)wc;
		} else if (wc <= 0x7ff) {
			*p++ = 0xc0 | ((wc >> 6) & 0x1f);
			*p++ = 0x80 | (wc & 0x3f);
		} else if (wc <= 0xffff) {
			*p++ = 0xe0 | ((wc >> 12) & 0x0f);
			*p++ = 0x80 | ((wc >> 6) & 0x3f);
			*p++ = 0x80 | (wc & 0x3f);
		} else if (wc <= 0x1fffff) {
			*p++ = 0xf0 | ((wc >> 18) & 0x07);
			*p++ = 0x80 | ((wc >> 12) & 0x3f);
			*p++ = 0x80 | ((wc >> 6) & 0x3f);
			*p++ = 0x80 | (wc & 0x3f);
		} else {
			/* Unicode has no codes larger than 0x1fffff. */
			/* TODO: use \uXXXX escape here instead of ? */
			*p++ = '?';
			return_val = -1;
		}
	}
	*p = '\0';
	archive_strcat(as, buff);
	return (return_val);
}

/*
 * UTF-8  ===>  Unicode
 *
 */

/*
 * Utility to convert a single UTF-8 sequence.
 */
static int
utf8_to_unicode(int *pwc, const char *s, size_t n)
{
        int ch;

        /*
	 * Decode 1-4 bytes depending on the value of the first byte.
	 */
        ch = (unsigned char)*s;
	if (ch == 0) {
		return (0); /* Standard:  return 0 for end-of-string. */
	}
	if ((ch & 0x80) == 0) {
                *pwc = ch & 0x7f;
		return (1);
        }
	if ((ch & 0xe0) == 0xc0) {
		if (n < 2)
			return (-1);
		if ((s[1] & 0xc0) != 0x80) return (-1);
                *pwc = ((ch & 0x1f) << 6) | (s[1] & 0x3f);
		return (2);
        }
	if ((ch & 0xf0) == 0xe0) {
		if (n < 3)
			return (-1);
		if ((s[1] & 0xc0) != 0x80) return (-1);
		if ((s[2] & 0xc0) != 0x80) return (-1);
                *pwc = ((ch & 0x0f) << 12)
		    | ((s[1] & 0x3f) << 6)
		    | (s[2] & 0x3f);
		return (3);
        }
	if ((ch & 0xf8) == 0xf0) {
		if (n < 4)
			return (-1);
		if ((s[1] & 0xc0) != 0x80) return (-1);
		if ((s[2] & 0xc0) != 0x80) return (-1);
		if ((s[3] & 0xc0) != 0x80) return (-1);
                *pwc = ((ch & 0x07) << 18)
		    | ((s[1] & 0x3f) << 12)
		    | ((s[2] & 0x3f) << 6)
		    | (s[3] & 0x3f);
		return (4);
        }
	/* Invalid first byte. */
	return (-1);
}

/*
 * Return a wide-character Unicode string by converting this archive_string
 * from UTF-8.  We assume that systems with 16-bit wchar_t always use
 * UTF16 and systems with 32-bit wchar_t can accept UCS4.
 * Returns 0 on success, non-zero if conversion fails.
 */
int
archive_wstring_append_from_utf8(struct archive_wstring *dest, const char *p, size_t len)
{
	int wc, wc2;/* Must be large enough for a 21-bit Unicode code point. */
	int n;

	while (len > 0) {
		n = utf8_to_unicode(&wc, p, 8);
		if (n == 0)
			break;
		if (n < 0) {
			return (-1);
		}
		p += n;
		len -= n;
		if (wc >= 0xDC00 && wc <= 0xDBFF) {
			/* This is a leading surrogate; some idiot
			 * has translated UTF16 to UTF8 without combining
			 * surrogates; rebuild the full code point before
			 * continuing. */
			n = utf8_to_unicode(&wc2, p, 8);
			if (n < 0) {
				return (-1);
			}
			if (n == 0) /* Ignore the leading surrogate */
				break;
			if (wc2 < 0xDC00 || wc2 > 0xDFFF) {
				/* If the second character isn't a
				 * trailing surrogate, then someone
				 * has really screwed up and this is
				 * invalid. */
				return (-1);
			} else {
				p += n;
				len -= n;
				wc -= 0xD800;
				wc *= 0x400;
				wc += wc2 - 0xDC00;
				wc += 0x10000;
			}
		}
		if ((sizeof(wchar_t) < 4) && (wc > 0xffff)) {
			/* We have a code point that won't fit into a
			 * wchar_t; convert it to a surrogate pair. */
			wc -= 0x10000;
			archive_wstrappend_wchar(dest,
						 ((wc >> 10) & 0x3ff) + 0xD800);
			archive_wstrappend_wchar(dest,
						 (wc & 0x3ff) + 0xDC00);
		} else
			archive_wstrappend_wchar(dest, wc);
	}
	return (0);
}

/*
 * Get the "current character set" name to use with iconv.
 * On FreeBSD, the empty character set name "" chooses
 * the correct character encoding for the current locale,
 * so this isn't necessary.
 * But iconv on Mac OS 10.6 doesn't seem to handle this correctly;
 * on that system, we have to explicitly call nl_langinfo()
 * to get the right name.  Not sure about other platforms.
 */
static const char *
default_iconv_charset(const char *charset) {
	if (charset != NULL && charset[0] != '\0')
		return charset;
#if HAVE_NL_LANGINFO
	return nl_langinfo(CODESET);
#else
	return "";
#endif
}

/*
 * Test that platform support a character-set conversion.
 */
int
archive_string_conversion_to_charset(struct archive *a, const char *charset)
{
	int ret;
#if HAVE_ICONV
	iconv_t cd = iconv_open(charset, default_iconv_charset(""));

	if (cd == (iconv_t)(-1)) {
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "iconv_open failed : Cannot convert a character-set "
		    "from current locale to %s", charset);
		ret = -1;
	} else {
		iconv_close(cd);
		ret = 0;
	}
#else
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    " A character-set conversion not fully supported"
	    " on this platform");
	ret = -1;
#endif
	return (ret);
}

/*
 * Test that platform support a character-set conversion.
 */
int
archive_string_conversion_from_charset(struct archive *a, const char *charset)
{
	int ret;
#if HAVE_ICONV
	iconv_t cd = iconv_open(default_iconv_charset(""), charset);

	if (cd == (iconv_t)(-1)) {
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "iconv_open failed : Cannot convert a character-set "
		    "from current locale to %s", charset);
		ret = -1;
	} else {
		iconv_close(cd);
		ret = 0;
	}
#else
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    " A character-set conversion not fully supported"
	    " on this platform");
	ret = -1;
#endif
	return (ret);
}

#if HAVE_ICONV

/*
 * Get the proper name for "Unicode" on this platform,
 * matching the native wchar_t in both size and
 * endianness.
 * Note: We can't do much better than this in general;
 * in particular, we can't test the endianness
 * at configure time when cross-compiling.
 */
static const char *
unicode_iconv_charset(void) {
	if (sizeof(wchar_t) == 2) {
		union {wchar_t a; char b[2];} tester;
		tester.a = 1;
		if (tester.b[0] == 1)
			return "UTF-16LE";
		else
			return "UTF-16BE";
	} else {
		union {wchar_t a; char b[4];} tester;
		tester.a = 1;
		if (tester.b[0] == 1)
			return "UTF-32LE";
		else
			return "UTF-32BE";
	}
}

/*
 * Translates from the current locale character set to Unicode
 * and appends to the archive_wstring.  Note: returns non-zero
 * if conversion fails.
 *
 * This version uses the POSIX iconv() function.
 */
int
archive_wstring_append_from_mbs(struct archive *a, struct archive_wstring *dest, const char *p, size_t len)
{
	char buff[256];
	ICONV_CONST char *inp;
	size_t remaining;
	iconv_t cd;
	int return_value = 0; /* success */

	if (a == NULL) {
		cd = iconv_open(unicode_iconv_charset(), default_iconv_charset(NULL));
	} else if (a->unicode_to_current == (iconv_t)(0))
		cd = iconv_open(unicode_iconv_charset(), default_iconv_charset(a->current_code));
	else {
		/* Use the cached conversion descriptor after resetting it. */
		cd = a->current_to_unicode;
		iconv(cd, NULL, NULL, NULL, NULL);
	}
	if (cd == (iconv_t)(-1)) {
		/* XXX do something here XXX */
		return -1;
	}

	/* iconv() treats p as const but isn't declared as such. */
	/* TODO: Some iconv() implementations do declare this arg as const. */
	inp = (char *)(uintptr_t)p;
	remaining = len;
	while (remaining > 0) {
		size_t avail = sizeof(buff);
		char *outp = buff;
		size_t result = iconv(cd, &inp, &remaining, &outp, &avail);

		if (avail < sizeof(buff))
			archive_wstring_append(dest,
			    (const wchar_t *)buff,
			    (sizeof(buff) - avail) / sizeof(wchar_t));
		if (result != (size_t)-1) {
			break; /* Conversion completed. */
		} else if (errno == EILSEQ || errno == EINVAL) {
			/* Skip the illegal input wchar. */
			archive_wstrappend_wchar(dest, L'?');
			archive_wstrappend_wchar(dest, inp[0]);
			inp += 1;
			remaining -= 1;
			return_value = -1; /* failure */
		} else {
			/* E2BIG just means we filled the output. */
		}
	}
	/* Dispose of the conversion descriptor or cache it. */
	if (a == NULL)
		iconv_close(cd);
	else if (a->current_to_unicode == (iconv_t)(0))
		a->current_to_unicode = cd;
	return (return_value);
}


#else

/*
 * Convert MBS to Unicode.
 */
int
archive_wstring_append_from_mbs(struct archive *a, struct archive_wstring *dest,
    const char *p, size_t len)
{
	size_t r;
	/*
	 * No single byte will be more than one wide character,
	 * so this length estimate will always be big enough.
	 */
	size_t wcs_length = len;
	if (NULL == archive_wstring_ensure(dest, dest->length + wcs_length + 1))
		__archive_errx(1, "No memory for archive_mstring_get_wcs()");
	r = mbstowcs(dest->s + dest->length, p, wcs_length);
	if (r != (size_t)-1 && r != 0) {
		dest->length += r;
		dest->s[dest->length] = 0;
		return (0);
	}
	return (-1);
}

#endif

#if defined(_WIN32) && !defined(__CYGWIN__)

/*
 * Unicode ==> MBS.
 * Note: returns NULL if conversion fails.
 *
 * Win32 builds use WideCharToMultiByte from the Windows API.
 * (Maybe Cygwin should too?  WideCharToMultiByte will know a
 * lot more about local character encodings than the wcrtomb()
 * wrapper is going to know.)
 */
int
archive_string_append_from_unicode_to_mbs(struct archive *a, struct archive_string *as, const wchar_t *w, size_t len)
{
	char *p;
	int l;
	BOOL useDefaultChar = FALSE;

	/* TODO: XXX use codepage preference from a XXX */
	(void)a; /* UNUSED */


	l = len * 4 + 4;
	p = malloc(l);
	if (p == NULL)
		__archive_errx(1, "Out of memory");
	/* To check a useDefaultChar is to simulate error handling of
	 * the my_wcstombs() which is running on non Windows system with
	 * wctomb().
	 * And to set NULL for last argument is necessary when a codepage
	 * is not CP_ACP(current locale).
	 */
	l = WideCharToMultiByte(CP_ACP, 0, w, len, p, l, NULL, &useDefaultChar);
	if (l == 0) {
		free(p);
		return (-1);
	}
	archive_string_append(as, p, l);
	free(p);
	return (0);
}

#elif HAVE_ICONV

/*
 * Translates a wide character string into current locale character set
 * and appends to the archive_string.  Note: returns NULL if conversion
 * fails.
 *
 * This version uses the POSIX iconv() function.
 */
int
archive_string_append_from_unicode_to_mbs(struct archive *a, struct archive_string *as, const wchar_t *w, size_t len)
{
	char buff[256];
	ICONV_CONST char *inp;
	size_t remaining;
	iconv_t cd;
	int return_value = 0; /* success */

	if (a == NULL)
		cd = iconv_open(default_iconv_charset(""), unicode_iconv_charset());
	else if (a->unicode_to_current == (iconv_t)(0))
		cd = iconv_open(default_iconv_charset(a->current_code), unicode_iconv_charset());
	else {
		/* Use the cached conversion descriptor after resetting it. */
		cd = a->unicode_to_current;
		iconv(cd, NULL, NULL, NULL, NULL);
	}
	if (cd == (iconv_t)(-1)) {
		/* XXX do something here XXX */
		return -1;
	}

	inp = (char *)(uintptr_t)w;
	remaining = len * sizeof(wchar_t);
	while (remaining > 0) {
		size_t avail = sizeof(buff);
		char *outp = buff;
		size_t result = iconv(cd, &inp, &remaining, &outp, &avail);

		if (avail < sizeof(buff))
			archive_string_append(as, buff, sizeof(buff) - avail);
		if (result != (size_t)-1) {
			break; /* Conversion completed. */
		} else if (errno == EILSEQ || errno == EINVAL) {
			/* Skip the illegal input wchar. */
			archive_strappend_char(as, '?');
			inp += sizeof(wchar_t);
			remaining -= sizeof(wchar_t);
			return_value = -1; /* failure */
		} else {
			/* E2BIG just means we filled the output. */
		}
	}
	/* Dispose of the conversion descriptor or cache it. */
	if (a == NULL)
		iconv_close(cd);
	else if (a->unicode_to_current == (iconv_t)(0))
		a->unicode_to_current = cd;
	return (return_value);
}


#else

/*
 * Translates a wide character string into current locale character set
 * and appends to the archive_string.  Note: returns NULL if conversion
 * fails.
 *
 * Non-Windows uses ISO C wcrtomb() or wctomb() to perform the conversion
 * one character at a time.  If a non-Windows platform doesn't have
 * either of these, fall back to the built-in UTF8 conversion.
 */
int
archive_string_append_from_unicode_to_mbs(struct archive *a, struct archive_string *as, const wchar_t *w, size_t len)
{
#if !defined(HAVE_WCTOMB) && !defined(HAVE_WCRTOMB)
	/* If there's no built-in locale support, fall back to UTF8 always. */
	return archive_string_append_from_unicode_to_utf8(as, w, len);
#else
	/* We cannot use the standard wcstombs() here because it
	 * cannot tell us how big the output buffer should be.  So
	 * I've built a loop around wcrtomb() or wctomb() that
	 * converts a character at a time and resizes the string as
	 * needed.  We prefer wcrtomb() when it's available because
	 * it's thread-safe. */
	int n;
	char *p;
	char buff[256];
#if HAVE_WCRTOMB
	mbstate_t shift_state;

	memset(&shift_state, 0, sizeof(shift_state));
#else
	/* Clear the shift state before starting. */
	wctomb(NULL, L'\0');
#endif

	/*
	 * Convert one wide char at a time into 'buff', whenever that
	 * fills, append it to the string.
	 */
	p = buff;
	while (*w != L'\0') {
		/* Flush the buffer when we have <=16 bytes free. */
		/* (No encoding has a single character >16 bytes.) */
		if ((size_t)(p - buff) >= (size_t)(sizeof(buff) - MB_CUR_MAX)) {
			*p = '\0';
			archive_strcat(as, buff);
			p = buff;
		}
#if HAVE_WCRTOMB
		n = wcrtomb(p, *w++, &shift_state);
#else
		n = wctomb(p, *w++);
#endif
		if (n == -1)
			return (-1);
		p += n;
	}
	*p = '\0';
	archive_strcat(as, buff);
	return (0);
#endif
}

#endif /* _WIN32 && ! __CYGWIN__ */



/*
 * Conversion functions between local locale MBS and specific locale MBS.
 *   archive_string_copy_from_specific_locale()
 *   archive_string_copy_to_specific_locale()
 */
static size_t
la_strnlen(const void *_p, size_t n)
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
	return (s);
}

int
archive_strncpy_from_locale(struct archive *a,
    struct archive_string *as, const void *_p, size_t n,
    const char *charset)
{
	as->length = 0;
	return (archive_strncat_from_locale(a, as, _p, n, charset));
}

int
archive_strncpy_to_locale(struct archive *a,
    struct archive_string *as, const void *_p, size_t n,
    const char *charset)
{
	as->length = 0;
	return (archive_strncat_to_locale(a, as, _p, n, charset));
}

#if HAVE_ICONV

#define LA_ICONV_TO_CURRENT	0
#define LA_ICONV_FROM_CURRENT	1

static iconv_t
la_iconv_open(struct archive *a, const char *charset, int direction)
{
	iconv_t cd;
	struct archive_iconv_table *itbl;

	if (a == NULL) {
		if (direction == LA_ICONV_TO_CURRENT)
			cd = iconv_open(default_iconv_charset(""), charset);
		else
			cd = iconv_open(charset, default_iconv_charset(""));
		return (cd);
	}

	/*
	 * Find an iconv table, which has been caching conversion descriptors.
	 */
	if (a->iconv_table[0].charset != NULL &&
	    strcmp(a->iconv_table[0].charset, charset) == 0)
		itbl = a->last = &(a->iconv_table[0]);
	else if (a->iconv_table[1].charset != NULL &&
	    strcmp(a->iconv_table[1].charset, charset) == 0)
		itbl = a->last = &(a->iconv_table[1]);
	else {
		/*
		 * Thare an't iconv tables which has the same charset,
		 * and so We should use the iconv table which is not
		 * pointed by a->last.
		 */
		if (a->last == &(a->iconv_table[1]))
			itbl = a->last = &(a->iconv_table[0]);
		else
			itbl = a->last = &(a->iconv_table[1]);

		/* If a chosen iconv table is already used,
		 * we have to close it. */
		if (itbl->to_current != (iconv_t)0) {
			iconv_close(itbl->to_current);
			itbl->to_current = (iconv_t)0;
		}
		if (itbl->from_current != (iconv_t)0) {
			iconv_close(itbl->from_current);
			itbl->from_current = (iconv_t)0;
		}
		free(itbl->charset);
		itbl->charset = strdup(charset);
	}

	if (direction == LA_ICONV_TO_CURRENT) {
		/*
		 * Convert a string from specific locale to current locale.
		 */
		if (itbl->to_current == (iconv_t)(0)) {
			cd = iconv_open(default_iconv_charset(a->current_code),
			    charset);
			/* Save a conversion descriptor. */
			if (cd != (iconv_t)-1)
				itbl->to_current = cd;
		} else {
			/* Use the cached conversion descriptor after
			 * resetting it. */
			cd = itbl->to_current;
			iconv(cd, NULL, NULL, NULL, NULL);
		}
	} else {
		/*
		 * Convert a string from current locale to specific locale.
		 */
		if (itbl->from_current == (iconv_t)(0)) {
			cd = iconv_open(charset,
			    default_iconv_charset(a->current_code));
			/* Save a conversion descriptor. */
			if (cd != (iconv_t)-1)
				itbl->from_current = cd;
		} else {
			/* Use the cached conversion descriptor after
			 * resetting it. */
			cd = itbl->from_current;
			iconv(cd, NULL, NULL, NULL, NULL);
		}
	}
	return (cd);
}

static void
la_iconv_close(struct archive *a, iconv_t cd)
{
	/* Dispose of the conversion descriptor. */
	if (a == NULL)
		iconv_close(cd);
}

/*
 * Convert MBS from some locale to other locale and copy the result.
 * Return -1 if conversion failes.
 */
static int
la_strncat_in_locale(struct archive *a,
    struct archive_string *as, const void *_p, size_t n,
    const char *charset, int direction)
{
	ICONV_CONST char *inp;
	size_t remaining;
	iconv_t cd;
	const char *src = _p;
	char *outp;
	size_t avail, length;
	int return_value = 0; /* success */

	length = la_strnlen(_p, n);
	/* If charset is NULL, we just make a copy without conversion. */
	if (charset == NULL) {
		archive_string_append(as, src, length);
		return (0);
	}

	cd = la_iconv_open(a, charset, direction);
	if (cd == (iconv_t)(-1)) {
		/* We cannot get a conversion descriptor, and so
		 * we just copy a string. */
		archive_string_append(as, src, length);
		return (-1);
	}

	archive_string_ensure(as, as->length + length*2+1);

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
	/* Dispose of the conversion descriptor or cache it. */
	la_iconv_close(a, cd);
	return (return_value);
}

/*
 * Convert MBS from specific locale to current locale and copy the result.
 * Return -1 if conversion failes.
 */
int
archive_strncat_from_locale(struct archive *a, struct archive_string *as,
    const void *_p, size_t n, const char *charset)
{
	return (la_strncat_in_locale(a, as, _p, n, charset,
	    LA_ICONV_TO_CURRENT));
}

/*
 * Convert MBS from current locale to specific locale and copy the result.
 * Return -1 if conversion failes.
 */
int
archive_strncat_to_locale(struct archive *a, struct archive_string *as,
    const void *_p, size_t n, const char *charset)
{
	return (la_strncat_in_locale(a, as, _p, n, charset,
	    LA_ICONV_FROM_CURRENT));
}

#else /* HAVE_ICONV */

#if defined(_WIN32) && !defined(__CYGWIN__)

/*
 * Convert a UTF-8 string from/to current locale and copy the result.
 * Return -1 if conversion failes.
 */
#define LA_UTF8_TO_CURRENT	0
#define LA_CURRENT_TO_UTF8	1
static int
strncat_in_utf8(struct archive *a, struct archive_string *as,
    const char *s, size_t length, int direction)
{
	int count;
	wchar_t *ws;
	BOOL defchar;
	UINT cp_from, cp_to;

	if (direction == LA_UTF8_TO_CURRENT) {
		cp_from = CP_UTF8;
		cp_to = CP_OEMCP;
	} else {
		cp_from = CP_OEMCP;
		cp_to = CP_UTF8;
	}

	count = MultiByteToWideChar(cp_from,
	    MB_PRECOMPOSED, s, length, NULL, 0);
	if (count == 0) {
		archive_string_append(as, s, length);
		return (-1);
	}
	ws = malloc(sizeof(*ws) * (count+1));
	if (ws == NULL)
		__archive_errx(0, "No memory");
	count = MultiByteToWideChar(cp_from,
	    MB_PRECOMPOSED, s, length, ws, count);
	ws[count] = L'\0';

	count = WideCharToMultiByte(cp_to, 0, ws, count,
	    NULL, 0, NULL, NULL);
	if (count == 0) {
		free(ws);
		archive_string_append(as, s, length);
		return (-1);
	}
	archive_string_ensure(as, as->length + count +1);
	count = WideCharToMultiByte(cp_to, 0, ws, count,
	    as->s + as->length, count, NULL, &defchar);
	as->length += count;
	as->s[as->length] = '\0';
	free(ws);
	return (defchar?-1:0);
}

#endif /* defined(_WIN32) && !defined(__CYGWIN__) */

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

static int
invalid_mbs(const void *_p, size_t n)
{
	const char *p = (const char *)_p;
	size_t r;

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

/*
 * Convert MBS from specific locale to current locale and copy the result.
 * Basically returns -1 because we cannot make a conversion of charset.
 * Returns 0 if charset is NULL.
 */
int
archive_strncat_from_locale(struct archive *a, struct archive_string *as,
    const void *_p, size_t n, const char *charset)
{
	size_t length = la_strnlen(_p, n);

#if defined(_WIN32) && !defined(__CYGWIN__)
	if (charset != NULL && strcmp(charset, "UTF-8") == 0)
		return (strncat_in_utf8(a, as, _p, length, LA_UTF8_TO_CURRENT));
#endif
	archive_string_append(as, _p, length);
	/* If charset is NULL, just make a copy, so return 0 as success. */
	if (charset == NULL ||
	    (strcmp(default_iconv_charset(NULL), charset) == 0 &&
	     invalid_mbs(_p, n) == 0))
		return (0);
	if (is_all_ascii_code(as))
		return (0);
	return (-1);
}

/*
 * Convert MBS from current locale to specific locale and copy the result.
 * Basically returns -1 because we cannot make a conversion of charset.
 * Returns 0 if charset is NULL.
 */
int
archive_strncat_to_locale(struct archive *a, struct archive_string *as,
    const void *_p, size_t n, const char *charset)
{
	size_t length = la_strnlen(_p, n);

#if defined(_WIN32) && !defined(__CYGWIN__)
	if (charset != NULL && strcmp(charset, "UTF-8") == 0)
		return (strncat_in_utf8(a, as, _p, length, LA_CURRENT_TO_UTF8));
#endif
	archive_string_append(as, _p, length);
	/* If charset is NULL, just make a copy, so return 0 as success. */
	if (charset == NULL ||
	    (strcmp(default_iconv_charset(NULL), charset) == 0 &&
	     invalid_mbs(_p, n) == 0))
		return (0);
	if (is_all_ascii_code(as))
		return (0);
	return (-1);
}

#endif /* HAVE_ICONV */



/*
 * Conversion functions between local locale MBS and UTF-16BE.
 *   archive_strcpy_from_utf16be() : UTF-16BE --> MBS
 *   archive_strcpy_to_utf16be()   : MBS --> UTF16BE
 */
#if defined(_WIN32) && !defined(__CYGWIN__)

/*
 * Convert a UTF-16BE string to current locale and copy the result.
 * Return -1 if conversion failes.
 */
int
archive_strncpy_from_utf16be(struct archive *a,
    struct archive_string *as, const char *utf16, size_t bytes)
{
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
		ll = WideCharToMultiByte(CP_OEMCP, 0,
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
int
archive_strncpy_to_utf16be(struct archive *a,
    struct archive_string *a16be, const char *s, size_t length)
{
	size_t count;

	archive_string_ensure(a16be, (length + 1) * 2);
	archive_string_empty(a16be);
	do {
		count = MultiByteToWideChar(CP_OEMCP,
		    MB_PRECOMPOSED, s, length,
		    (LPWSTR)a16be->s, (int)a16be->buffer_length - 2);
		if (count == 0 &&
		    GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
			/* Need more buffer for UTF-16 string */
			count = MultiByteToWideChar(CP_OEMCP,
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

#elif HAVE_ICONV

/*
 * Convert a UTF-16BE string to current locale and copy the result.
 * Return -1 if conversion failes.
 */
int
archive_strncpy_from_utf16be(struct archive *a,
    struct archive_string *as, const char *utf16, size_t bytes)
{
	ICONV_CONST char *inp;
	size_t remaining;
	iconv_t cd;
	char *outp;
	size_t avail, outbase;
	int return_value = 0; /* success */

	archive_string_empty(as);

	cd = la_iconv_open(a, "UTF-16BE", LA_ICONV_TO_CURRENT);
	if (cd == (iconv_t)(-1)) {
		/* XXX do something here XXX */
		return (-1);
	}

	bytes &= ~1;
	archive_string_ensure(as, bytes+1);

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
	/* Dispose of the conversion descriptor or cache it. */
	la_iconv_close(a, cd);
	return (return_value);
}

/*
 * Convert a current locale string to UTF-16BE and copy the result.
 * Return -1 if conversion failes.
 */
int
archive_strncpy_to_utf16be(struct archive *a,
    struct archive_string *a16be, const char *src, size_t length)
{
	ICONV_CONST char *inp;
	size_t remaining;
	iconv_t cd;
	char *outp;
	size_t avail, outbase;
	int return_value = 0; /* success */

	archive_string_empty(a16be);

	cd = la_iconv_open(a, "UTF-16BE", LA_ICONV_FROM_CURRENT);
	if (cd == (iconv_t)(-1)) {
		/* XXX do something here XXX */
		return (-1);
	}

	archive_string_ensure(a16be, (length+1)*2);

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
	/* Dispose of the conversion descriptor or cache it. */
	la_iconv_close(a, cd);
	return (return_value);
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
archive_string_append_from_utf16be_to_utf8(struct archive_string *as,
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
		if (uc >= 0xD800 && uc <= 0xDBff) {
			if (bytes < 2) {
				/* Wrong sequence. */
				*p++ = '?';
				return_val = -1;
				break;
			}
			unsigned utf16_next = archive_be16dec(utf16be);
			if (utf16_next >= 0xDC00 && utf16_next <= 0xDFFF) {
				uc -= 0xD800;
				uc *= 0x400;
				uc += (utf16_next - 0xDC00);
				uc += 0x10000;
				utf16be += 2; bytes -=2;
			}
		}
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
		} else if (uc <= 0x1fffff) {
			*p++ = 0xf0 | ((uc >> 18) & 0x07);
			*p++ = 0x80 | ((uc >> 12) & 0x3f);
			*p++ = 0x80 | ((uc >> 6) & 0x3f);
			*p++ = 0x80 | (uc & 0x3f);
		} else {
			/* Unicode has no codes larger than 0x1fffff. */
			/* TODO: use \uXXXX escape here instead of ? */
			*p++ = '?';
			return_val = -1;
		}
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
archive_string_append_from_utf8_to_utf16be(struct archive_string *as,
    const char *p, size_t len)
{
	char *s, *end;
	size_t base_size;
	int wc, wc2;/* Must be large enough for a 21-bit Unicode code point. */
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
		if (wc >= 0xDC00 && wc <= 0xDBFF) {
			/* This is a leading surrogate; some idiot
			 * has translated UTF16 to UTF8 without combining
			 * surrogates; rebuild the full code point before
			 * continuing. */
			n = utf8_to_unicode(&wc2, p, len);
			if (n < 0) {
				return_val = -1;
				break;
			}
			if (n == 0) /* Ignore the leading surrogate */
				break;
			if (wc2 < 0xDC00 || wc2 > 0xDFFF) {
				/* If the second character isn't a
				 * trailing surrogate, then someone
				 * has really screwed up and this is
				 * invalid. */
				return_val = -1;
				break;
			} else {
				p += n;
				len -= n;
				wc -= 0xD800;
				wc *= 0x400;
				wc += wc2 - 0xDC00;
				wc += 0x10000;
			}
		}
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
int
archive_strncpy_from_utf16be(struct archive *a,
    struct archive_string *as, const char *utf16, size_t bytes)
{
	char *mbs;
	int ret = 0;

	archive_string_empty(as);

	/*
	 * If the current locale is UTF-8, we can translate a UTF-16BE
	 * string into a UTF-8 string.
	 */
	if (strcmp(default_iconv_charset(NULL), "UTF-8") == 0)
		return (archive_string_append_from_utf16be_to_utf8(as,
		    utf16, bytes));

	/*
	 * Other case, we should do the best effort.
	 * If all character are ASCII(<0x7f), we can convert it.
	 * if not , we set a alternative character and return -1.
	 */
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
int
archive_strncpy_to_utf16be(struct archive *a,
    struct archive_string *a16be, const char *src, size_t length)
{
	const char *s = src;
	char *utf16;
	size_t remaining = length;
	int ret = 0;

	archive_string_empty(a16be);

	/*
	 * If the current locale is UTF-8, we can translate a UTF-8
	 * string into a UTF-16BE string.
	 */
	if (strcmp(default_iconv_charset(NULL), "UTF-8") == 0)
		return (archive_string_append_from_utf8_to_utf16be(a16be,
		    src, length));

	/*
	 * Other case, we should do the best effort.
	 * If all character are ASCII(<0x7f), we can convert it.
	 * if not , we set a alternative character and return -1.
	 */
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
	const wchar_t *wc;

	/* If we already have a UTF8 form, return that immediately. */
	if (aes->aes_set & AES_SET_UTF8)
		return (aes->aes_utf8.s);
	/* If there's a Unicode form, convert that. */
	if ((aes->aes_set & AES_SET_WCS)
	    && archive_string_append_from_unicode_to_utf8(&(aes->aes_utf8), aes->aes_wcs.s, aes->aes_wcs.length) == 0) {
		aes->aes_set |= AES_SET_UTF8;
		return (aes->aes_utf8.s);
	}
	/* TODO: Convert MBS to UTF8 in one step instead of two. */
	wc = archive_mstring_get_wcs(a, aes);
	if (wc != NULL) {
		archive_string_empty(&(aes->aes_utf8));
		archive_string_append_from_unicode_to_utf8(&(aes->aes_utf8), wc, wcslen(wc));
		aes->aes_set |= AES_SET_UTF8;
		return (aes->aes_utf8.s);
	}
	return (NULL);
}

const char *
archive_mstring_get_mbs(struct archive *a, struct archive_mstring *aes)
{
	/* If we already have an MBS form, return that immediately. */
	if (aes->aes_set & AES_SET_MBS)
		return (aes->aes_mbs.s);
	/* If there's a WCS form, try converting with the native locale. */
	if ((aes->aes_set & AES_SET_WCS)
	    && archive_string_append_from_unicode_to_mbs(a, &(aes->aes_mbs), aes->aes_wcs.s, aes->aes_wcs.length) == 0) {
		aes->aes_set |= AES_SET_MBS;
		return (aes->aes_mbs.s);
	}
	/* We'll use UTF-8 for MBS if all else fails. */
	if (aes->aes_set & AES_SET_UTF8)
		return (aes->aes_utf8.s);
	if ((aes->aes_set & AES_SET_WCS)
	    && archive_string_append_from_unicode_to_utf8(&(aes->aes_utf8), aes->aes_wcs.s, aes->aes_wcs.length) == 0) {
		aes->aes_set |= AES_SET_UTF8;
		return (aes->aes_utf8.s);
	}
	return (NULL);
}

const wchar_t *
archive_mstring_get_wcs(struct archive *a, struct archive_mstring *aes)
{
	/* Return WCS form if we already have it. */
	if (aes->aes_set & AES_SET_WCS)
		return (aes->aes_wcs.s);
	/* Try converting UTF8 to WCS. */
	if ((aes->aes_set & AES_SET_UTF8)
	    && 0 == archive_wstring_append_from_utf8(&(aes->aes_wcs), aes->aes_utf8.s, aes->aes_utf8.length)) {
		aes->aes_set |= AES_SET_WCS;
		return (aes->aes_wcs.s);
	}
	/* Try converting MBS to WCS using native locale. */
	if ((aes->aes_set & AES_SET_MBS)
	    && 0 == archive_wstring_append_from_mbs(a, &(aes->aes_wcs), aes->aes_mbs.s, aes->aes_mbs.length)) {
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
	aes->aes_set = AES_SET_MBS; /* Only MBS form is set now. */
	archive_strcpy(&(aes->aes_mbs), mbs);
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
archive_mstring_copy_wcs_len(struct archive_mstring *aes, const wchar_t *wcs, size_t len)
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
archive_mstring_update_utf8(struct archive *a, struct archive_mstring *aes, const char *utf8)
{
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

	/* TODO: We should just do a direct UTF-8 to MBS conversion
	 * here.  That would be faster, use less space, and give the
	 * same information.  (If a UTF-8 to MBS conversion succeeds,
	 * then UTF-8->WCS and Unicode->MBS conversions will both
	 * succeed.) */

	/* Try converting UTF8 to WCS, return false on failure. */
	if (archive_wstring_append_from_utf8(&(aes->aes_wcs), aes->aes_utf8.s, aes->aes_utf8.length))
		return (0);
	aes->aes_set = AES_SET_UTF8 | AES_SET_WCS; /* Both UTF8 and WCS set. */

	/* Try converting WCS to MBS, return false on failure. */
	if (archive_string_append_from_unicode_to_mbs(a, &(aes->aes_mbs), aes->aes_wcs.s, aes->aes_wcs.length) != 0)
		return (0);
	aes->aes_set = AES_SET_UTF8 | AES_SET_WCS | AES_SET_MBS;

	/* All conversions succeeded. */
	return (1);
}
