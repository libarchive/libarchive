/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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

#include "archive_private.h"
#include "archive_string.h"

#if SIZEOF_WCHAR_T == 2
#define __LA_UNICODE "UCS-2-INTERNAL"
#else
#define __LA_UNICODE "UCS-4-INTERNAL"
#endif

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
 * Note: returns NULL if conversion fails, but still leaves a best-effort
 * conversion in the argument as.
 */
struct archive_string *
archive_strappend_w_utf8(struct archive_string *as, const wchar_t *w)
{
	char *p;
	unsigned wc;
	char buff[256];
	struct archive_string *return_val = as;

	/*
	 * Convert one wide char at a time into 'buff', whenever that
	 * fills, append it to the string.
	 */
	p = buff;
	while (*w != L'\0') {
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
			return_val = NULL;
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
archive_wstrappend_utf8(struct archive_wstring *dest, struct archive_string *src)
{
	int wc, wc2;/* Must be large enough for a 21-bit Unicode code point. */
	const char *p;
	int n;

	p = src->s;
	while (*p != '\0') {
		n = utf8_to_unicode(&wc, p, 8);
		if (n == 0)
			break;
		if (n < 0) {
			return (-1);
		}
		p += n;
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

#if HAVE_ICONV

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
 * Translates from the current locale character set to Unicode
 * and appends to the archive_wstring.  Note: returns NULL if conversion
 * fails.
 *
 * This version uses the POSIX iconv() function.
 */
int
archive_wstrcpy_mbs(struct archive *a, struct archive_wstring *dest, struct archive_string *src)
{
	char *p;
	char buff[256];
	size_t remaining;
	iconv_t cd;

	if (a == NULL) {
		cd = iconv_open(__LA_UNICODE, default_iconv_charset(NULL));
	} else if (a->unicode_to_current == (iconv_t)(0))
		cd = iconv_open(__LA_UNICODE, default_iconv_charset(a->current_code));
	else {
		/* Use the cached conversion descriptor after resetting it. */
		cd = a->current_to_unicode;
		iconv(cd, NULL, NULL, NULL, NULL);
	}
	if (cd == (iconv_t)(-1)) {
		/* XXX do something here XXX */
		return -1;
	}

	p = src->s;
	remaining = archive_strlen(src);
	for (;;) {
		size_t avail = sizeof(buff);
		char *outp = buff;
		size_t result = iconv(cd, &p, &remaining, &outp, &avail);

		if (avail < sizeof(buff))
			archive_wstring_append(dest,
			    (const wchar_t *)buff,
			    (sizeof(buff) - avail) / sizeof(wchar_t));
		if (result != (size_t)-1) {
			break; /* Conversion completed. */
		} else if (errno == EILSEQ) {
			/* Skip the illegal input wchar. */
			archive_wstrappend_wchar(dest, L'?');
			archive_wstrappend_wchar(dest, p[0]);
			p += 1;
			remaining -= 1;
		} else if (errno == E2BIG) {
			/* Flush the output and do some more. */
			continue;
		} else if (errno == EINVAL) {
			/* Final character is invalid. */
			archive_wstrappend_wchar(dest, L'?');
			archive_wstrappend_wchar(dest, p[0]);
			break;
		}
	}
	/* Dispose of the conversion descriptor or cache it. */
	if (a == NULL)
		iconv_close(cd);
	else if (a->current_to_unicode == (iconv_t)(0))
		a->current_to_unicode = cd;
	return (0);
}


#else

/*
 * Convert MBS to Unicode.
 */
int
archive_wstrcpy_mbs(struct archive *a, struct archive_wstring *dest,
			 struct archive_string *src)
{
	size_t r;
	/*
	 * No single byte will be more than one wide character,
	 * so this length estimate will always be big enough.
	 */
	size_t wcs_length = src->length;
	if (NULL == archive_wstring_ensure(dest, wcs_length + 1))
		__archive_errx(1, "No memory for archive_mstring_get_wcs()");
	r = mbstowcs(dest->s, src->s, wcs_length);
	if (r != (size_t)-1 && r != 0) {
		dest->s[r] = 0;
		dest->length = r;
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
struct archive_string *
archive_strappend_w_mbs(struct archive *a, struct archive_string *as, const wchar_t *w)
{
	char *p;
	int l, wl;
	BOOL useDefaultChar = FALSE;

	/* TODO: XXX use codepage preference from a XXX */
	(void)a; /* UNUSED */

	wl = (int)wcslen(w);
	l = wl * 4 + 4;
	p = malloc(l);
	if (p == NULL)
		__archive_errx(1, "Out of memory");
	/* To check a useDefaultChar is to simulate error handling of
	 * the my_wcstombs() which is running on non Windows system with
	 * wctomb().
	 * And to set NULL for last argument is necessary when a codepage
	 * is not CP_ACP(current locale).
	 */
	l = WideCharToMultiByte(CP_ACP, 0, w, wl, p, l, NULL, &useDefaultChar);
	if (l == 0) {
		free(p);
		return (NULL);
	}
	archive_string_append(as, p, l);
	free(p);
	return (as);
}

#elif HAVE_ICONV

/*
 * Translates a wide character string into current locale character set
 * and appends to the archive_string.  Note: returns NULL if conversion
 * fails.
 *
 * This version uses the POSIX iconv() function.
 */
struct archive_string *
archive_strappend_w_mbs(struct archive *a, struct archive_string *as, const wchar_t *w)
{
	char *p;
	char buff[256];
	size_t remaining;
	iconv_t cd;

	if (a == NULL)
		cd = iconv_open(default_iconv_charset(""), __LA_UNICODE);
	else if (a->unicode_to_current == (iconv_t)(0))
		cd = iconv_open(default_iconv_charset(a->current_code), __LA_UNICODE);
	else {
		/* Use the cached conversion descriptor after resetting it. */
		cd = a->unicode_to_current;
		iconv(cd, NULL, NULL, NULL, NULL);
	}
	if (cd == (iconv_t)(-1)) {
		/* XXX do something here XXX */
		return NULL;
	}

	p = (char *)(uintptr_t)w;
	remaining = wcslen(w) * sizeof(wchar_t);
	for (;;) {
		size_t avail = sizeof(buff);
		char *outp = buff;
		size_t result = iconv(cd, &p, &remaining, &outp, &avail);

		if (avail < sizeof(buff))
			archive_string_append(as, buff, sizeof(buff) - avail);
		if (result >= 0) {
			break; /* Conversion completed. */
		} else if (errno == EILSEQ) {
			/* Skip the illegal input wchar. */
			archive_strappend_char(as, '?');
			p += sizeof(wchar_t);
			remaining -= sizeof(wchar_t);
		} else if (errno == E2BIG) {
			/* Flush the output and do some more. */
			continue;
		} else if (errno == EINVAL) {
			/* Final character is invalid. */
			archive_strappend_char(as, '?');
			break;
		}
	}
	/* Dispose of the conversion descriptor or cache it. */
	if (a == NULL)
		iconv_close(cd);
	else if (a->unicode_to_current == (iconv_t)(0))
		a->unicode_to_current = cd;
	return (as);
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
struct archive_string *
archive_strappend_w_mbs(struct archive *a, struct archive_string *as, const wchar_t *w)
{
#if !defined(HAVE_WCTOMB) && !defined(HAVE_WCRTOMB)
	/* If there's no built-in locale support, fall back to UTF8 always. */
	return archive_strappend_w_utf8(as, w);
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
			return (NULL);
		p += n;
	}
	*p = '\0';
	archive_strcat(as, buff);
	return (as);
#endif
}

#endif /* _WIN32 && ! __CYGWIN__ */


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
	    && archive_strappend_w_utf8(&(aes->aes_utf8), aes->aes_wcs.s) != NULL) {
		aes->aes_set |= AES_SET_UTF8;
		return (aes->aes_utf8.s);
	}
	/* TODO: Convert MBS to UTF8 in one step instead of two. */
	wc = archive_mstring_get_wcs(a, aes);
	if (wc != NULL) {
		archive_string_empty(&(aes->aes_utf8));
		archive_strappend_w_utf8(&(aes->aes_utf8), wc);
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
	    && archive_strappend_w_mbs(a, &(aes->aes_mbs), aes->aes_wcs.s) != NULL) {
		aes->aes_set |= AES_SET_MBS;
		return (aes->aes_mbs.s);
	}
	/* We'll use UTF-8 for MBS if all else fails. */
	if (aes->aes_set & AES_SET_UTF8)
		return (aes->aes_utf8.s);
	if ((aes->aes_set & AES_SET_WCS)
	    && archive_strappend_w_utf8(&(aes->aes_utf8), aes->aes_wcs.s) != NULL) {
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
	    && !archive_wstrappend_utf8(&(aes->aes_wcs), &(aes->aes_utf8))) {
		aes->aes_set |= AES_SET_WCS;
		return (aes->aes_wcs.s);
	}
	/* Try converting MBS to WCS using native locale. */
	if ((aes->aes_set & AES_SET_MBS)
	    && !archive_wstrcpy_mbs(a, &(aes->aes_wcs), &(aes->aes_mbs))) {
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
	if (archive_wstrappend_utf8(&(aes->aes_wcs), &(aes->aes_utf8)))
		return (0);
	aes->aes_set = AES_SET_UTF8 | AES_SET_WCS; /* Both UTF8 and WCS set. */

	/* Try converting WCS to MBS, return false on failure. */
	if (archive_strappend_w_mbs(a, &(aes->aes_mbs), aes->aes_wcs.s) == NULL)
		return (0);
	aes->aes_set = AES_SET_UTF8 | AES_SET_WCS | AES_SET_MBS;

	/* All conversions succeeded. */
	return (1);
}
