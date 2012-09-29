/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

#include "bsdtar_platform.h"
__FBSDID("$FreeBSD: src/usr.bin/tar/util.c,v 1.23 2008/12/15 06:00:25 kientzle Exp $");

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>  /* Linux doesn't define mode_t, etc. in sys/stat.h. */
#endif
#include <ctype.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#else
/* If we don't have wctype, we need to hack up some version of iswprint(). */
#define	iswprint isprint
#endif

#include "bsdtar.h"
#include "err.h"

static size_t	bsdtar_expand_char(char *, size_t, char);
static const char *strip_components(const char *path, int elements);

#if defined(_WIN32) && !defined(__CYGWIN__)
#define	read _read
#endif

/* TODO:  Hack up a version of mbtowc for platforms with no wide
 * character support at all.  I think the following might suffice,
 * but it needs careful testing.
 * #if !HAVE_MBTOWC
 * #define	mbtowc(wcp, p, n) ((*wcp = *p), 1)
 * #endif
 */

/*
 * Print a string, taking care with any non-printable characters.
 *
 * Note that we use a stack-allocated buffer to receive the formatted
 * string if we can.  This is partly performance (avoiding a call to
 * malloc()), partly out of expedience (we have to call vsnprintf()
 * before malloc() anyway to find out how big a buffer we need; we may
 * as well point that first call at a small local buffer in case it
 * works), but mostly for safety (so we can use this to print messages
 * about out-of-memory conditions).
 */

void
safe_fprintf(FILE *f, const char *fmt, ...)
{
	char fmtbuff_stack[256]; /* Place to format the printf() string. */
	char outbuff[256]; /* Buffer for outgoing characters. */
	char *fmtbuff_heap; /* If fmtbuff_stack is too small, we use malloc */
	char *fmtbuff;  /* Pointer to fmtbuff_stack or fmtbuff_heap. */
	int fmtbuff_length;
	int length, n;
	va_list ap;
	const char *p;
	unsigned i;
	wchar_t wc;
	char try_wc;

	/* Use a stack-allocated buffer if we can, for speed and safety. */
	fmtbuff_heap = NULL;
	fmtbuff_length = sizeof(fmtbuff_stack);
	fmtbuff = fmtbuff_stack;

	/* Try formatting into the stack buffer. */
	va_start(ap, fmt);
	length = vsnprintf(fmtbuff, fmtbuff_length, fmt, ap);
	va_end(ap);

	/* If the result was too large, allocate a buffer on the heap. */
	while (length < 0 || length >= fmtbuff_length) {
		if (length >= fmtbuff_length)
			fmtbuff_length = length+1;
		else if (fmtbuff_length < 8192)
			fmtbuff_length *= 2;
		else {
			int old_length = fmtbuff_length;
			/* Convert to unsigned to avoid signed overflow,
			 * otherwise the check may be optimized away. */
			fmtbuff_length += (unsigned)fmtbuff_length / 4;
			if (old_length > fmtbuff_length) {
				length = old_length;
				fmtbuff_heap[length-1] = '\0';
				break;
			}
		}
		free(fmtbuff_heap);
		fmtbuff_heap = malloc(fmtbuff_length);

		/* Reformat the result into the heap buffer if we can. */
		if (fmtbuff_heap != NULL) {
			fmtbuff = fmtbuff_heap;
			va_start(ap, fmt);
			length = vsnprintf(fmtbuff, fmtbuff_length, fmt, ap);
			va_end(ap);
		} else {
			/* Leave fmtbuff pointing to the truncated
			 * string in fmtbuff_stack. */
			length = sizeof(fmtbuff_stack) - 1;
			break;
		}
	}

	/* Note: mbrtowc() has a cleaner API, but mbtowc() seems a bit
	 * more portable, so we use that here instead. */
	if (mbtowc(NULL, NULL, 1) == -1) { /* Reset the shift state. */
		/* NOTE: This case may not happen, but it needs to be compiled
		 * safely without warnings by both gcc on linux and clang. */
		if (fmtbuff_heap != NULL)
			free(fmtbuff_heap);
		return;
	}

	/* Write data, expanding unprintable characters. */
	p = fmtbuff;
	i = 0;
	try_wc = 1;
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
					i += (unsigned)bsdtar_expand_char(
					    outbuff, i, *p++);
			}
		} else {
			/* After any conversion failure, don't bother
			 * trying to convert the rest. */
			i += (unsigned)bsdtar_expand_char(outbuff, i, *p++);
			try_wc = 0;
		}

		/* If our output buffer is full, dump it and keep going. */
		if (i > (sizeof(outbuff) - 20)) {
			outbuff[i] = '\0';
			fprintf(f, "%s", outbuff);
			i = 0;
		}
	}
	outbuff[i] = '\0';
	fprintf(f, "%s", outbuff);

	/* If we allocated a heap-based formatting buffer, free it now. */
	if (fmtbuff_heap != NULL)
		free(fmtbuff_heap);
}

/*
 * Render an arbitrary sequence of bytes into printable ASCII characters.
 */
static size_t
bsdtar_expand_char(char *buff, size_t offset, char c)
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
			sprintf(buff + i, "%03o", 0xFF & (int)c);
			i += 3;
		}
	}

	return (i - offset);
}

int
yes(const char *fmt, ...)
{
	char buff[32];
	char *p;
	ssize_t l;

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, " (y/N)? ");
	fflush(stderr);

	l = read(2, buff, sizeof(buff) - 1);
	if (l < 0) {
	  fprintf(stderr, "Keyboard read failed\n");
	  exit(1);
	}
	if (l == 0)
		return (0);
	buff[l] = 0;

	for (p = buff; *p != '\0'; p++) {
		if (isspace((unsigned char)*p))
			continue;
		switch(*p) {
		case 'y': case 'Y':
			return (1);
		case 'n': case 'N':
			return (0);
		default:
			return (0);
		}
	}

	return (0);
}

/*-
 * The logic here for -C <dir> attempts to avoid
 * chdir() as long as possible.  For example:
 * "-C /foo -C /bar file"          needs chdir("/bar") but not chdir("/foo")
 * "-C /foo -C bar file"           needs chdir("/foo/bar")
 * "-C /foo -C bar /file1"         does not need chdir()
 * "-C /foo -C bar /file1 file2"   needs chdir("/foo/bar") before file2
 *
 * The only correct way to handle this is to record a "pending" chdir
 * request and combine multiple requests intelligently until we
 * need to process a non-absolute file.  set_chdir() adds the new dir
 * to the pending list; do_chdir() actually executes any pending chdir.
 *
 * This way, programs that build tar command lines don't have to worry
 * about -C with non-existent directories; such requests will only
 * fail if the directory must be accessed.
 *
 */
void
set_chdir(struct bsdtar *bsdtar, const char *newdir)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	if (newdir[0] == '/' || newdir[0] == '\\' ||
	    /* Detect this type, for example, "C:\" or "C:/" */
	    (((newdir[0] >= 'a' && newdir[0] <= 'z') ||
	      (newdir[0] >= 'A' && newdir[0] <= 'Z')) &&
	    newdir[1] == ':' && (newdir[2] == '/' || newdir[2] == '\\'))) {
#else
	if (newdir[0] == '/') {
#endif
		/* The -C /foo -C /bar case; dump first one. */
		free(bsdtar->pending_chdir);
		bsdtar->pending_chdir = NULL;
	}
	if (bsdtar->pending_chdir == NULL)
		/* Easy case: no previously-saved dir. */
		bsdtar->pending_chdir = strdup(newdir);
	else {
		/* The -C /foo -C bar case; concatenate */
		char *old_pending = bsdtar->pending_chdir;
		size_t old_len = strlen(old_pending);
		bsdtar->pending_chdir = malloc(old_len + strlen(newdir) + 2);
		if (old_pending[old_len - 1] == '/')
			old_pending[old_len - 1] = '\0';
		if (bsdtar->pending_chdir != NULL)
			sprintf(bsdtar->pending_chdir, "%s/%s",
			    old_pending, newdir);
		free(old_pending);
	}
	if (bsdtar->pending_chdir == NULL)
		lafe_errc(1, errno, "No memory");
}

void
do_chdir(struct bsdtar *bsdtar)
{
	if (bsdtar->pending_chdir == NULL)
		return;

	if (chdir(bsdtar->pending_chdir) != 0) {
		lafe_errc(1, 0, "could not chdir to '%s'\n",
		    bsdtar->pending_chdir);
	}
	free(bsdtar->pending_chdir);
	bsdtar->pending_chdir = NULL;
}

static const char *
strip_components(const char *p, int elements)
{
	/* Skip as many elements as necessary. */
	while (elements > 0) {
		switch (*p++) {
		case '/':
#if defined(_WIN32) && !defined(__CYGWIN__)
		case '\\': /* Support \ path sep on Windows ONLY. */
#endif
			elements--;
			break;
		case '\0':
			/* Path is too short, skip it. */
			return (NULL);
		}
	}

	/* Skip any / characters.  This handles short paths that have
	 * additional / termination.  This also handles the case where
	 * the logic above stops in the middle of a duplicate //
	 * sequence (which would otherwise get converted to an
	 * absolute path). */
	for (;;) {
		switch (*p) {
		case '/':
#if defined(_WIN32) && !defined(__CYGWIN__)
		case '\\': /* Support \ path sep on Windows ONLY. */
#endif
			++p;
			break;
		case '\0':
			return (NULL);
		default:
			return (p);
		}
	}
}

/*
 * Handle --strip-components and any future path-rewriting options.
 * Returns non-zero if the pathname should not be extracted.
 *
 * TODO: Support pax-style regex path rewrites.
 */
int
edit_pathname(struct bsdtar *bsdtar, struct archive_entry *entry)
{
	const char *name = archive_entry_pathname(entry);
#if HAVE_REGEX_H
	char *subst_name;
	int r;

	r = apply_substitution(bsdtar, name, &subst_name, 0, 0);
	if (r == -1) {
		lafe_warnc(0, "Invalid substitution, skipping entry");
		return 1;
	}
	if (r == 1) {
		archive_entry_copy_pathname(entry, subst_name);
		if (*subst_name == '\0') {
			free(subst_name);
			return -1;
		} else
			free(subst_name);
		name = archive_entry_pathname(entry);
	}

	if (archive_entry_hardlink(entry)) {
		r = apply_substitution(bsdtar, archive_entry_hardlink(entry), &subst_name, 0, 1);
		if (r == -1) {
			lafe_warnc(0, "Invalid substitution, skipping entry");
			return 1;
		}
		if (r == 1) {
			archive_entry_copy_hardlink(entry, subst_name);
			free(subst_name);
		}
	}
	if (archive_entry_symlink(entry) != NULL) {
		r = apply_substitution(bsdtar, archive_entry_symlink(entry), &subst_name, 1, 0);
		if (r == -1) {
			lafe_warnc(0, "Invalid substitution, skipping entry");
			return 1;
		}
		if (r == 1) {
			archive_entry_copy_symlink(entry, subst_name);
			free(subst_name);
		}
	}
#endif

	/* Strip leading dir names as per --strip-components option. */
	if (bsdtar->strip_components > 0) {
		const char *linkname = archive_entry_hardlink(entry);

		name = strip_components(name, bsdtar->strip_components);
		if (name == NULL)
			return (1);

		if (linkname != NULL) {
			linkname = strip_components(linkname,
			    bsdtar->strip_components);
			if (linkname == NULL)
				return (1);
			archive_entry_copy_hardlink(entry, linkname);
		}
	}

	/* By default, don't write or restore absolute pathnames. */
	if (!bsdtar->option_absolute_paths) {
		const char *rp, *p = name;
		int slashonly = 1;

		/* Remove leading "//./" or "//?/" or "//?/UNC/"
		 * (absolute path prefixes used by Windows API) */
		if ((p[0] == '/' || p[0] == '\\') &&
		    (p[1] == '/' || p[1] == '\\') &&
		    (p[2] == '.' || p[2] == '?') &&
		    (p[3] == '/' || p[3] == '\\'))
		{
			if (p[2] == '?' &&
			    (p[4] == 'U' || p[4] == 'u') &&
			    (p[5] == 'N' || p[5] == 'n') &&
			    (p[6] == 'C' || p[6] == 'c') &&
			    (p[7] == '/' || p[7] == '\\'))
				p += 8;
			else
				p += 4;
			slashonly = 0;
		}
		do {
			rp = p;
			/* Remove leading drive letter from archives created
			 * on Windows. */
			if (((p[0] >= 'a' && p[0] <= 'z') ||
			     (p[0] >= 'A' && p[0] <= 'Z')) &&
				 p[1] == ':') {
				p += 2;
				slashonly = 0;
			}
			/* Remove leading "/../", "//", etc. */
			while (p[0] == '/' || p[0] == '\\') {
				if (p[1] == '.' && p[2] == '.' &&
					(p[3] == '/' || p[3] == '\\')) {
					p += 3; /* Remove "/..", leave "/"
							 * for next pass. */
					slashonly = 0;
				} else
					p += 1; /* Remove "/". */
			}
		} while (rp != p);

		if (p != name && !bsdtar->warned_lead_slash) {
			/* Generate a warning the first time this happens. */
			if (slashonly)
				lafe_warnc(0,
				    "Removing leading '%c' from member names",
				    name[0]);
			else
				lafe_warnc(0,
				    "Removing leading drive letter from "
				    "member names");
			bsdtar->warned_lead_slash = 1;
		}

		/* Special case: Stripping everything yields ".". */
		if (*p == '\0')
			name = ".";
		else
			name = p;
	} else {
		/* Strip redundant leading '/' characters. */
		while (name[0] == '/' && name[1] == '/')
			name++;
	}

	/* Safely replace name in archive_entry. */
	if (name != archive_entry_pathname(entry)) {
		char *q = strdup(name);
		archive_entry_copy_pathname(entry, q);
		free(q);
	}
	return (0);
}

/*
 * It would be nice to just use printf() for formatting large numbers,
 * but the compatibility problems are quite a headache.  Hence the
 * following simple utility function.
 */
const char *
tar_i64toa(int64_t n0)
{
	static char buff[24];
	uint64_t n = n0 < 0 ? -n0 : n0;
	char *p = buff + sizeof(buff);

	*--p = '\0';
	do {
		*--p = '0' + (int)(n % 10);
	} while (n /= 10);
	if (n0 < 0)
		*--p = '-';
	return p;
}

/*
 * Like strcmp(), but try to be a little more aware of the fact that
 * we're comparing two paths.  Right now, it just handles leading
 * "./" and trailing '/' specially, so that "a/b/" == "./a/b"
 *
 * TODO: Make this better, so that "./a//b/./c/" == "a/b/c"
 * TODO: After this works, push it down into libarchive.
 * TODO: Publish the path normalization routines in libarchive so
 * that bsdtar can normalize paths and use fast strcmp() instead
 * of this.
 *
 * Note: This is currently only used within write.c, so should
 * not handle \ path separators.
 */

int
pathcmp(const char *a, const char *b)
{
	/* Skip leading './' */
	if (a[0] == '.' && a[1] == '/' && a[2] != '\0')
		a += 2;
	if (b[0] == '.' && b[1] == '/' && b[2] != '\0')
		b += 2;
	/* Find the first difference, or return (0) if none. */
	while (*a == *b) {
		if (*a == '\0')
			return (0);
		a++;
		b++;
	}
	/*
	 * If one ends in '/' and the other one doesn't,
	 * they're the same.
	 */
	if (a[0] == '/' && a[1] == '\0' && b[0] == '\0')
		return (0);
	if (a[0] == '\0' && b[0] == '/' && b[1] == '\0')
		return (0);
	/* They're really different, return the correct sign. */
	return (*(const unsigned char *)a - *(const unsigned char *)b);
}
