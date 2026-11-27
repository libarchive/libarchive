/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */

#include "lafe_platform.h"

#include <ctype.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#else
/* If we don't have wctype, we need to hack up some version of iswprint(). */
#define	iswprint isprint
#endif

#include "lafe_safe_fputs.h"

static size_t	lafe_expand_char(char *, size_t, size_t, char);

/* TODO:  Hack up a version of mbtowc for platforms with no wide
 * character support at all.  I think the following might suffice,
 * but it needs careful testing.
 * #if !HAVE_MBTOWC
 * #define	mbtowc(wcp, p, n) ((*wcp = *p), 1)
 * #endif
 */

/* Print a string, taking care of any non-printable characters. */

void
safe_fputs(const char *s, FILE *f)
{
	char outbuff[256]; /* Buffer for outgoing characters. */
	int n;
	const char *p;
	size_t i, length;
	wchar_t wc;
	char try_wc;

	if (s == NULL) {
		fputs("(null)", f);
		return;
	}

	/* Note: mbrtowc() has a cleaner API, but mbtowc() seems a bit
	 * more portable, so we use that here instead. */
	if (mbtowc(NULL, NULL, 1) == -1) { /* Reset the shift state. */
		/* mbtowc() should never fail in practice, but
		 * handle the theoretical error anyway. */
		return;
	}

	p = s;

	/* Directly print as many ASCII characters as possible. */
	if (mbtowc(NULL, NULL, 0) == 0) {
		while (*p != '\0' && isprint((unsigned char)*p) &&
		    (unsigned char)*p < 128 && *p != '\\')
			p++;
		fwrite(s, 1, p - s, f);
	}

	/* Write data, expanding unprintable characters. */
	i = 0;
	try_wc = 1;
	length = strlen(p);
	while (*p != '\0') {

		/* Convert to wide char, test if the wide
		 * char is printable in the current locale. */
		if (try_wc && (n = mbtowc(&wc, p, length)) != -1) {
			length -= n;
			if (iswprint(wc) && wc != L'\\') {
				/* Printable, copy the bytes through. */
				while (n-- > 0)
					outbuff[i++] = *p++;
			} else {
				/* Not printable, format the bytes. */
				while (n-- > 0)
					i += lafe_expand_char(
					    outbuff, sizeof(outbuff), i, *p++);
			}
		} else {
			/* After any conversion failure, don't bother
			 * trying to convert the rest. */
			i += lafe_expand_char(outbuff, sizeof(outbuff), i, *p++);
			try_wc = 0;
		}

		/* If our output buffer is full, dump it and keep going. */
		if (i > (sizeof(outbuff) - 128)) {
			fwrite(outbuff, 1, i, f);
			i = 0;
		}
	}
	fwrite(outbuff, 1, i, f);
}

/*
 * Render an arbitrary sequence of bytes into printable ASCII characters.
 */
static size_t
lafe_expand_char(char *buff, size_t buffsize, size_t offset, char c)
{
	size_t i = offset;

	if (isprint((unsigned char)c) && c != '\\')
		buff[i++] = c;
	else {
		buff[i++] = '\\';
		switch (c) {
		case '\a': buff[i++] = 'a'; break;
		case '\b': buff[i++] = 'b'; break;
		case '\f': buff[i++] = 'f'; break;
		case '\n': buff[i++] = 'n'; break;
#if '\r' != '\n'
		/* On some platforms, \n and \r are the same. */
		case '\r': buff[i++] = 'r'; break;
#endif
		case '\t': buff[i++] = 't'; break;
		case '\v': buff[i++] = 'v'; break;
		case '\\': buff[i++] = '\\'; break;
		default:
			snprintf(buff + i, buffsize - i, "%03o",
			    0xFF & (unsigned int)c);
			i += 3;
		}
	}

	return (i - offset);
}
