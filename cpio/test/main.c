/*
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

/*
 * Various utility routines useful for test programs.
 * Each test program is linked against this file.
 */
#include "test.h"

#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <time.h>
#if defined(_WIN32) && !defined(__CYGWIN__)
#if !defined(__GNUC__)
#include <crtdbg.h>
#endif
#include <io.h>
#include <windows.h>
#ifndef F_OK
#define F_OK (0)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m)  ((m) & _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m)  ((m) & _S_IFREG)
#endif
#define access _access
#define chdir _chdir
#ifndef fileno
#define fileno _fileno
#endif
//#define fstat _fstat64
#define getcwd _getcwd
#define lstat stat
//#define lstat _stat64
//#define stat _stat64
#define rmdir _rmdir
#define strdup _strdup
#define umask _umask
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
void *GetFunctionKernel32(const char *name)
{
	static HINSTANCE lib;
	static int set;
	if (!set) {
		set = 1;
		lib = LoadLibrary("kernel32.dll");
	}
	if (lib == NULL) {
		fprintf(stderr, "Can't load kernel32.dll?!\n");
		return NULL;
	}
	return (void *)GetProcAddress(lib, name);
}

int __CreateSymbolicLinkA(const char *linkname, const char *target, int flags)
{
	static BOOLEAN (*f)(LPCSTR, LPCSTR, DWORD);
	static int set;
	if (!set) {
		set = 1;
		f = GetFunctionKernel32("CreateSymbolicLinkA");
	}
	return f == NULL ? 0 : (*f)(linkname, target, flags);
}

#endif

/*
 * This same file is used pretty much verbatim for all test harnesses.
 *
 * The next few lines are the only differences.
 */
#define	PROGRAM "bsdcpio" /* Name of program being tested. */
#undef LIBRARY		  /* Not testing a library. */
#define ENVBASE "BSDCPIO" /* Prefix for environment variables. */
#undef	EXTRA_DUMP	     /* How to dump extra data */
/* How to generate extra version info. */
#define	EXTRA_VERSION    (systemf("%s --version", testprog) ? "" : "")
#define KNOWNREF	"test_option_f.cpio.uu"
__FBSDID("$FreeBSD: src/usr.bin/cpio/test/main.c,v 1.3 2008/08/24 04:58:22 kientzle Exp $");

/*
 * "list.h" is simply created by "grep DEFINE_TEST"; it has
 * a line like
 *      DEFINE_TEST(test_function)
 * for each test.
 * Include it here with a suitable DEFINE_TEST to declare all of the
 * test functions.
 */
#undef DEFINE_TEST
#define	DEFINE_TEST(name) void name(void);
#include "list.h"

/* Enable core dump on failure. */
static int dump_on_failure = 0;
/* Default is to remove temp dirs for successful tests. */
static int keep_temp_files = 0;
/* Default is to print some basic information about each test. */
static int quiet_flag = 0;
/* Default is to summarize repeated failures. */
static int verbose = 0;
/* Cumulative count of component failures. */
static int failures = 0;
/* Cumulative count of skipped component tests. */
static int skips = 0;
/* Cumulative count of assertions. */
static int assertions = 0;

/* Directory where uuencoded reference files can be found. */
static const char *refdir;


#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__GNUC__)

static void
invalid_parameter_handler(const wchar_t * expression,
    const wchar_t * function, const wchar_t * file,
    unsigned int line, uintptr_t pReserved)
{
	/* nop */
}

#endif

/*
 * My own implementation of the standard assert() macro emits the
 * message in the same format as GCC (file:line: message).
 * It also includes some additional useful information.
 * This makes it a lot easier to skim through test failures in
 * Emacs.  ;-)
 *
 * It also supports a few special features specifically to simplify
 * test harnesses:
 *    failure(fmt, args) -- Stores a text string that gets
 *          printed if the following assertion fails, good for
 *          explaining subtle tests.
 */
static char msgbuff[4096];
static const char *msg, *nextmsg;

/*
 * For each test source file, we remember how many times each
 * failure was reported.
 */
static const char *failed_filename = NULL;
static struct line {
	int count;
	int skip;
}  failed_lines[10000];

/*
 * Called at the beginning of each assert() function.
 */
static void
count_assertion(const char *file, int line)
{
	(void)file; /* UNUSED */
	(void)line; /* UNUSED */
	++assertions;
	msg = nextmsg;
	nextmsg = NULL;
	/* Uncomment to print file:line after every assertion.
	 * Verbose, but occasionally useful in tracking down crashes. */
	/* printf("Checked %s:%d\n", file, line); */
}

/*
 * Count this failure; return true if this failure is being reported.
 */
static int
report_failure(const char *filename, int line, const char *fmt, ...)
{
	++failures;

	/* If this is a new file, clear the counts. */
	if (failed_filename == NULL || strcmp(failed_filename, filename) != 0)
		memset(failed_lines, 0, sizeof(failed_lines));
	failed_filename = filename;

	/* Report first hit always, every hit if verbose. */
	if (failed_lines[line].count++ == 0 || verbose) {
		va_list ap;
		va_start(ap, fmt);
		fprintf(stderr, "%s:%d: ", filename, line);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		fprintf(stderr, "\n");
		return (1);
	}
	return (0);
}

/* Complete reporting of failed tests. */
static void
finish_failure(void *extra)
{
	if (msg != NULL && msg[0] != '\0')
		fprintf(stderr, "   Description: %s\n", msg);

#ifdef EXTRA_DUMP
	if (extra != NULL)
		fprintf(stderr, "   detail: %s\n", EXTRA_DUMP(extra));
#else
	(void)extra; /* UNUSED */
#endif

	if (dump_on_failure) {
		fprintf(stderr,
		    " *** forcing core dump so failure can be debugged ***\n");
		*(char *)(NULL) = 0;
		exit(1);
	}
}

/*
 * Copy arguments into file-local variables.
 */
static const char *test_filename;
static int test_line;
static void *test_extra;
void test_setup(const char *filename, int line)
{
	test_filename = filename;
	test_line = line;
}

/*
 * Inform user that we're skipping a test.
 */
void
test_skipping(const char *fmt, ...)
{
	va_list ap;

	if (report_failure(test_filename, test_line, "SKIPPING" )) {
		va_start(ap,fmt);
		fprintf(stderr, "      Reason: ");
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
		/* Don't finish_failure() here. */
	}
	/* Mark as skip, so doesn't count as failed test. */
	failed_lines[test_line].skip = 1;
	++skips;
	--failures;
}

/*
 * Summarize repeated failures in the just-completed test file.
 * The reports above suppress multiple failures from the same source
 * line; this reports on any tests that did fail multiple times.
 */
static void
summarize(void)
{
	unsigned int i;

	for (i = 0; i < sizeof(failed_lines)/sizeof(failed_lines[0]); i++) {
		if (failed_lines[i].count == 0)
			break;
		if (failed_lines[i].count > 1 && !failed_lines[i].skip)
			fprintf(stderr, "%s:%d: Failed %d times\n",
			    failed_filename, i, failed_lines[i].count);
	}
	/* Clear the failure history for the next file. */
	memset(failed_lines, 0, sizeof(failed_lines));
}

/* Set up a message to display only after a test fails. */
void
failure(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsprintf(msgbuff, fmt, ap);
	va_end(ap);
	nextmsg = msgbuff;
}

/* Generic assert() just displays the failed condition. */
int
test_assert(const char *file, int line, int value,
    const char *condition, void *extra)
{
	count_assertion(file, line);
	if (value)
		return (value);
	if (report_failure(file, line, "Assertion failed: %s", condition)) {
		finish_failure(extra);
	}
	return (value);
}

int
test_assert_chdir(const char *file, int line, const char *pathname)
{
	count_assertion(file, line);
	if (chdir(pathname) == 0)
		return (1);
	if (report_failure(file, line, "chdir(\"%s\")", pathname)) {
		finish_failure(NULL);
	}
	return (0);

}

/* assertEqualInt() displays the values of the two integers. */
int
test_assert_equal_int(const char *file, int line,
    long long v1, const char *e1, long long v2, const char *e2, void *extra)
{
	count_assertion(file, line);
	if (v1 == v2)
		return (1);
	if (report_failure(file, line, "Ints not equal")) {
		fprintf(stderr, "      %s=%lld\n", e1, v1);
		fprintf(stderr, "      %s=%lld\n", e2, v2);
		finish_failure(extra);
	}
	return (0);
}

static void strdump(const char *e, const char *p)
{
	fprintf(stderr, "      %s = ", e);
	if (p == NULL) {
		fprintf(stderr, "(null)");
		return;
	}
	fprintf(stderr, "\"");
	while (*p != '\0') {
		unsigned int c = 0xff & *p++;
		switch (c) {
		case '\a': fprintf(stderr, "\a"); break;
		case '\b': fprintf(stderr, "\b"); break;
		case '\n': fprintf(stderr, "\n"); break;
		case '\r': fprintf(stderr, "\r"); break;
		default:
			if (c >= 32 && c < 127)
				fprintf(stderr, "%c", c);
			else
				fprintf(stderr, "\\x%02X", c);
		}
	}
	fprintf(stderr, "\"");
	fprintf(stderr, " (length %d)\n", p == NULL ? 0 : (int)strlen(p));
}

/* assertEqualString() displays the values of the two strings. */
int
test_assert_equal_string(const char *file, int line,
    const char *v1, const char *e1,
    const char *v2, const char *e2,
    void *extra)
{
	count_assertion(file, line);
	if (v1 == v2)
		return (1);
	else if (strcmp(v1, v2) == 0)
		return (1);
	if (report_failure(file, line, "Strings not equal")) {
		strdump(e1, v1);
		strdump(e2, v2);
		finish_failure(extra);
	}
	return (0);
}

static void wcsdump(const char *e, const wchar_t *w)
{
	fprintf(stderr, "      %s = ", e);
	if (w == NULL) {
		fprintf(stderr, "(null)");
		return;
	}
	fprintf(stderr, "\"");
	while (*w != L'\0') {
		unsigned int c = *w++;
		if (c >= 32 && c < 127)
			fprintf(stderr, "%c", c);
		else if (c < 256)
			fprintf(stderr, "\\x%02X", c);
		else if (c < 0x10000)
			fprintf(stderr, "\\u%04X", c);
		else
			fprintf(stderr, "\\U%08X", c);
	}
	fprintf(stderr, "\"\n");
}

/* assertEqualWString() displays the values of the two strings. */
int
test_assert_equal_wstring(const char *file, int line,
    const wchar_t *v1, const char *e1,
    const wchar_t *v2, const char *e2,
    void *extra)
{
	count_assertion(file, line);
	if (v1 == v2)
		return (1);
	else if (wcscmp(v1, v2) == 0)
		return (1);
	if (report_failure(file, line, "Unicode strings not equal")) {
		wcsdump(e1, v1);
		wcsdump(e2, v2);
		finish_failure(extra);
	}
	return (0);
}

/*
 * Pretty standard hexdump routine.  As a bonus, if ref != NULL, then
 * any bytes in p that differ from ref will be highlighted with '_'
 * before and after the hex value.
 */
static void
hexdump(const char *p, const char *ref, size_t l, size_t offset)
{
	size_t i, j;
	char sep;

	for(i=0; i < l; i+=16) {
		fprintf(stderr, "%04x", (unsigned)(i + offset));
		sep = ' ';
		for (j = 0; j < 16 && i + j < l; j++) {
			if (ref != NULL && p[i + j] != ref[i + j])
				sep = '_';
			fprintf(stderr, "%c%02x", sep, 0xff & (int)p[i+j]);
			if (ref != NULL && p[i + j] == ref[i + j])
				sep = ' ';
		}
		for (; j < 16; j++) {
			fprintf(stderr, "%c  ", sep);
			sep = ' ';
		}
		fprintf(stderr, "%c", sep);
		for (j=0; j < 16 && i + j < l; j++) {
			int c = p[i + j];
			if (c >= ' ' && c <= 126)
				fprintf(stderr, "%c", c);
			else
				fprintf(stderr, ".");
		}
		fprintf(stderr, "\n");
	}
}

/* assertEqualMem() displays the values of the two memory blocks. */
int
test_assert_equal_mem(const char *file, int line,
    const void *_v1, const char *e1,
    const void *_v2, const char *e2,
    size_t l, const char *ld, void *extra)
{
	const char *v1 = (const char *)_v1;
	const char *v2 = (const char *)_v2;
	size_t offset;

	count_assertion(file, line);
	if (v1 == v2)
		return (1);
	else if (memcmp(v1, v2, l) == 0)
		return (1);
	if (report_failure(file, line, "Memory not equal")) {
		fprintf(stderr, "      size %s = %d\n", ld, (int)l);
		/* Dump 48 bytes (3 lines) so that the first difference is
		 * in the second line. */
		offset = 0;
		while (l > 64 && memcmp(v1, v2, 32) == 0) {
			/* Two lines agree, so step forward one line. */
			v1 += 16;
			v2 += 16;
			l -= 16;
			offset += 16;
		}
		fprintf(stderr, "      Dump of %s\n", e1);
		hexdump(v1, v2, l < 64 ? l : 64, offset);
		fprintf(stderr, "      Dump of %s\n", e2);
		hexdump(v2, v1, l < 64 ? l : 64, offset);
		fprintf(stderr, "\n");
		finish_failure(extra);
	}
	return (0);
}

int
test_assert_empty_file(const char *f1fmt, ...)
{
	char buff[1024];
	char f1[1024];
	struct stat st;
	va_list ap;
	ssize_t s;
	FILE *f;

	count_assertion(test_filename, test_line);
	va_start(ap, f1fmt);
	vsprintf(f1, f1fmt, ap);
	va_end(ap);

	if (stat(f1, &st) != 0) {
		if (report_failure(test_filename, test_line, "Stat failed: %s", f1)) {
			finish_failure(NULL);
		}
		return (0);
	}
	if (st.st_size == 0)
		return (1);

	if (report_failure(test_filename, test_line, "%s not empty", f1)) {
		fprintf(stderr, "    File size: %d\n", (int)st.st_size);
		fprintf(stderr, "    Contents:\n");
		f = fopen(f1, "rb");
		if (f == NULL) {
			fprintf(stderr, "    Unable to open %s\n", f1);
		} else {
			s = ((off_t)sizeof(buff) < st.st_size) ?
			    (ssize_t)sizeof(buff) : (ssize_t)st.st_size;
			s = fread(buff, 1, s, f);
			hexdump(buff, NULL, s, 0);
			fclose(f);
		}
		finish_failure(NULL);
	}
	return (0);
}

int
test_assert_non_empty_file(const char *f1fmt, ...)
{
	char f1[1024];
	struct stat st;
	va_list ap;

	count_assertion(test_filename, test_line);
	va_start(ap, f1fmt);
	vsprintf(f1, f1fmt, ap);
	va_end(ap);

	if (stat(f1, &st) != 0) {
		if (report_failure(test_filename, test_line, "Stat failed: %s", f1))
			finish_failure(NULL);
		return (0);
	}
	if (st.st_size != 0)
		return (1);
	if (report_failure(test_filename, test_line, "File empty: %s", f1))
		finish_failure(NULL);
	return (0);
}

/* assertEqualFile() asserts that two files have the same contents. */
/* TODO: hexdump the first bytes that actually differ. */
int
test_assert_equal_file(const char *fn1, const char *f2pattern, ...)
{
	char fn2[1024];
	va_list ap;
	char buff1[1024];
	char buff2[1024];
	FILE *f1, *f2;
	int n1, n2;

	count_assertion(test_filename, test_line);
	va_start(ap, f2pattern);
	vsprintf(fn2, f2pattern, ap);
	va_end(ap);

	f1 = fopen(fn1, "rb");
	f2 = fopen(fn2, "rb");
	for (;;) {
		n1 = fread(buff1, 1, sizeof(buff1), f1);
		n2 = fread(buff2, 1, sizeof(buff2), f2);
		if (n1 != n2)
			break;
		if (n1 == 0 && n2 == 0) {
			fclose(f1);
			fclose(f2);
			return (1);
		}
		if (memcmp(buff1, buff2, n1) != 0)
			break;
	}
	fclose(f1);
	fclose(f2);
	if (report_failure(test_filename, test_line, "Files not identical")) {
		fprintf(stderr, "  file1=\"%s\"\n", fn1);
		fprintf(stderr, "  file2=\"%s\"\n", fn2);
		finish_failure(test_extra);
	}
	return (0);
}

int
test_assert_file_exists(const char *fpattern, ...)
{
	char f[1024];
	va_list ap;

	count_assertion(test_filename, test_line);
	va_start(ap, fpattern);
	vsprintf(f, fpattern, ap);
	va_end(ap);

#if defined(_WIN32) && !defined(__CYGWIN__)
	if (!_access(f, 0))
		return (1);
#else
	if (!access(f, F_OK))
		return (1);
#endif
	if (report_failure(test_filename, test_line, "File doesn't exist")) {
		fprintf(stderr, "  file=\"%s\"\n", f);
		finish_failure(test_extra);
	}
	return (0);
}

int
test_assert_file_not_exists(const char *fpattern, ...)
{
	char f[1024];
	va_list ap;

	count_assertion(test_filename, test_line);
	va_start(ap, fpattern);
	vsprintf(f, fpattern, ap);
	va_end(ap);

#if defined(_WIN32) && !defined(__CYGWIN__)
	if (_access(f, 0))
		return (1);
#else
	if (access(f, F_OK))
		return (1);
#endif
	if (report_failure(test_filename, test_line, "File exists")) {
		fprintf(stderr, "  file=\"%s\"\n", f);
		finish_failure(test_extra);
	}
	return (0);
}

/* assertFileContents() asserts the contents of a file. */
int
test_assert_file_contents(const void *buff, int s, const char *fpattern, ...)
{
	char fn[1024];
	va_list ap;
	char *contents;
	FILE *f;
	int n;

	count_assertion(test_filename, test_line);
	va_start(ap, fpattern);
	vsprintf(fn, fpattern, ap);
	va_end(ap);

	f = fopen(fn, "rb");
	if (f == NULL) {
		if (report_failure(test_filename, test_line, "File doesn't exist: %s", fn)) {
			finish_failure(test_extra);
		}
		return (0);
	}
	contents = malloc(s * 2);
	n = fread(contents, 1, s * 2, f);
	fclose(f);
	if (n == s && memcmp(buff, contents, s) == 0) {
		free(contents);
		return (1);
	}
	if (report_failure(test_filename, test_line, "File contents don't match")) {
		fprintf(stderr, "  file=\"%s\"\n", fn);
		if (n > 0)
			hexdump(contents, buff, n > 512 ? 512 : 0, 0);
		else {
			fprintf(stderr, "  File empty, contents should be:\n");
			hexdump(buff, NULL, s > 512 ? 512 : 0, 0);
		}
		finish_failure(test_extra);
	}
	free(contents);
	return (0);
}

/* assertTextFileContents() asserts the contents of a text file. */
int
test_assert_text_file_contents(const char *buff, const char *fn)
{
	char *contents;
	const char *btxt, *ftxt;
	FILE *f;
	int n, s;

	count_assertion(test_filename, test_line);
	f = fopen(fn, "r");
	s = strlen(buff);
	contents = malloc(s * 2 + 128);
	n = fread(contents, 1, s * 2 + 128 - 1, f);
	if (n >= 0)
		contents[n] = '\0';
	fclose(f);
	/* Compare texts. */
	btxt = buff;
	ftxt = (const char *)contents;
	while (*btxt != '\0' && *ftxt != '\0') {
		if (*btxt == *ftxt) {
			++btxt;
			++ftxt;
			continue;
		}
		if (btxt[0] == '\n' && ftxt[0] == '\r' && ftxt[1] == '\n') {
			/* Pass over different new line characters. */
			++btxt;
			ftxt += 2;
			continue;
		}
		break;
	}
	if (*btxt == '\0' && *ftxt == '\0') {
		free(contents);
		return (1);
	}
	if (report_failure(test_filename, test_line, "Contents don't match")) {
		fprintf(stderr, "  file=\"%s\"\n", fn);
		if (n > 0)
			hexdump(contents, buff, n, 0);
		else {
			fprintf(stderr, "  File empty, contents should be:\n");
			hexdump(buff, NULL, s, 0);
		}
		finish_failure(test_extra);
	}
	free(contents);
	return (0);
}

int
test_assert_file_hardlinks(const char *file, int line,
						   const char *path1, const char *path2)
{
	struct stat st1, st2;
	int r;

	count_assertion(file, line);
	r = lstat(path1, &st1);
	if (r != 0) {
		if (report_failure(file, line, "File %s should exist", path1))
			finish_failure(NULL);
		return (0);
	}
	r = lstat(path2, &st2);
	if (r != 0) {
		if (report_failure(file, line, "File %s should exist", path2))
			finish_failure(NULL);
		return (0);
	}
	if (st1.st_ino != st2.st_ino || st1.st_dev != st2.st_dev) {
		if (report_failure(file, line,
			"Files %s and %s are not hardlinked", path1, path2))
			finish_failure(NULL);
		return (0);
	}
	return (1);
}

int
test_assert_file_nlinks(const char *file, int line,
    const char *pathname, int nlinks)
{
	struct stat st;
	int r;

	count_assertion(file, line);
	r = lstat(pathname, &st);
	if (r == 0 && st.st_nlink == nlinks)
			return (1);
	if (report_failure(file, line, "File %s has %d links, expected %d",
		pathname, st.st_nlink, nlinks))
		finish_failure(NULL);
	return (0);
}

int
test_assert_file_size(const char *file, int line,
    const char *pathname, long size)
{
	struct stat st;
	int r;

	count_assertion(file, line);
	r = lstat(pathname, &st);
	if (r == 0 && st.st_size == size)
			return (1);
	if (report_failure(file, line, "File %s has size %ld, expected %ld",
		pathname, (long)st.st_size, (long)size))
		finish_failure(NULL);
	return (0);
}

int
test_assert_is_dir(const char *file, int line, const char *pathname, int mode)
{
	struct stat st;
	int r;

	count_assertion(file, line);
	r = lstat(pathname, &st);
	if (r != 0) {
		if (report_failure(file, line, "Dir %s doesn't exist", pathname))
			finish_failure(NULL);
		return (0);
	}
	if (!S_ISDIR(st.st_mode)) {
		if (report_failure(file, line, "%s is not a dir", pathname))
			finish_failure(NULL);
		return (0);
	}
	if (mode < 0)
		return (1);
	if (mode != (st.st_mode & 07777)) {
		if (report_failure(file, line, "Dir %s has wrong mode", pathname)) {
			fprintf(stderr, "  Expected: 0%3o\n", mode);
			fprintf(stderr, "  Found: 0%3o\n", st.st_mode & 07777);
			finish_failure(NULL);
		}
		return (0);
	}
	return (1);
}

int
test_assert_is_symlink(const char *file, int line,
    const char *pathname, const char *contents)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	count_assertion(file, line);
	// TODO: Vista supports symlinks
	if (report_failure(file, line, "Symlink %s not supported", pathname))
		finish_failure(NULL);
	return (0);
#else
	char buff[300];
	struct stat st;
	ssize_t linklen;
	int r;

	count_assertion(file, line);
	r = lstat(pathname, &st);
	if (r != 0) {
		if (report_failure(file, line, "Symlink %s doesn't exist", pathname))
			finish_failure(NULL);
		return (0);
	}
	if (!S_ISLNK(st.st_mode)) {
		if (report_failure(file, line, "%s should be a symlink", pathname))
			finish_failure(NULL);
		return (0);
	}
	if (contents == NULL)
		return (1);
	linklen = readlink(pathname, buff, sizeof(buff));
	if (linklen < 0) {
		if (report_failure(file, line, "Can't read symlink %s", pathname))
			finish_failure(NULL);
		return (0);
	}
	buff[linklen] = '\0';
	if (strcmp(buff, contents) != 0) {
		if (report_failure(file, line, "Wrong symlink %s", pathname)) {
			fprintf(stderr, "   Expected: %s\n", contents);
			fprintf(stderr, "   Found: %s\n", buff);
			finish_failure(NULL);
		}
		return (0);
	}
	return (1);
#endif
}

int
test_assert_is_reg(const char *file, int line, const char *pathname, int mode)
{
	struct stat st;
	int r;

	count_assertion(file, line);
	r = lstat(pathname, &st);
	if (r != 0 || !S_ISREG(st.st_mode)) {
		if (report_failure(file, line, "File %s doesn't exist", pathname))
			finish_failure(NULL);
		return (0);
	}
	if (mode < 0)
		return (1);
	if (mode != (st.st_mode & 07777)) {
		if (report_failure(file, line, "File %s has wrong mode", pathname)) {
			fprintf(stderr, "  Expected: 0%3o\n", mode);
			fprintf(stderr, "  Found: 0%3o\n", st.st_mode & 07777);
			finish_failure(NULL);
		}
		return (0);
	}
	return (1);
}

int
test_assert_make_dir(const char *file, int line, const char *dirname, int mode)
{
	count_assertion(file, line);
#if defined(_WIN32) && !defined(__CYGWIN__)
	if (0 == _mkdir(dirname))
		return (1);
#else
	if (0 == mkdir(dirname, mode))
		return (1);
#endif
	if (report_failure(file, line, "Could not create directory %s", dirname))
		finish_failure(NULL);
	return(0);
}

int
test_assert_make_file(const char *file, int line,
    const char *path, int mode, const char *contents)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	/* TODO: Rework this to set file mode as well. */
	FILE *f;
	count_assertion(file, line);
	f = fopen(path, "wb");
	if (f == NULL) {
		if (report_failure(file, line, "Could not create file %s", path))
			finish_failure(NULL);
		return (0);
	}
	if (contents != NULL) {
		if (strlen(contents)
		    != fwrite(contents, 1, strlen(contents), f)) {
			fclose(f);
			if (report_failure(file, line, "Could not write file %s", path))
				finish_failure(NULL);
			return (0);
		}
	}
	fclose(f);
	return (1);
#else
	int fd;
	count_assertion(file, line);
	fd = open(path, O_CREAT | O_WRONLY, mode >= 0 ? mode : 0644);
	if (fd < 0) {
		if (report_failure(file, line, "Could not create %s", path))
			finish_failure(NULL);
		return (0);
	}
	if (contents != NULL) {
		if ((ssize_t)strlen(contents)
		    != write(fd, contents, strlen(contents))) {
			close(fd);
			if (report_failure(file, line, "Could not write to %s", path))
				finish_failure(NULL);
			return (0);
		}
	}
	close(fd);
	return (1);
#endif
}

int
test_assert_make_hardlink(const char *file, int line,
    const char *newpath, const char *linkto)
{
	int succeeded;

	count_assertion(file, line);
#if HAVE_LINK
	succeeded = !link(linkto, newpath);
#elif HAVE_CREATEHARDLINKA
	succeeded = CreateHardLinkA(newpath, linkto, NULL);
#else
	succeeded = 0;
#endif
	if (succeeded)
		return (1);
	if (report_failure(file, line, "Could not create hardlink")) {
		fprintf(stderr, "   New link: %s\n", newpath);
		fprintf(stderr, "   Old name: %s\n", linkto);
		finish_failure(NULL);
	}
	return(0);
}


int
test_assert_make_symlink(const char *file, int line,
    const char *newpath, const char *linkto)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	int targetIsDir = 0;  /* TODO: Fix this */
	count_assertion(file, line);
	if (__CreateSymbolicLinkA(newpath, linkto, targetIsDir))
		return (1);
#elif HAVE_SYMLINK
	count_assertion(file, line);
	if (0 == symlink(linkto, newpath))
		return (1);
#endif
	if (report_failure(file, line, "Could not create symlink")) {
		fprintf(stderr, "   New link: %s\n", newpath);
		fprintf(stderr, "   Old name: %s\n", linkto);
		finish_failure(NULL);
	}
	return(0);
}

int
test_assert_umask(const char *file, int line, int mask)
{
	count_assertion(file, line);
	(void)file; /* UNUSED */
	(void)line; /* UNUSED */
	umask(mask);
	return (1);
}

void
sleepUntilAfter(time_t t)
{
	while (t >= time(NULL))
#if defined(_WIN32) && !defined(__CYGWIN__)
		Sleep(500);
#else
		sleep(1);
#endif
}

/*
 * Call standard system() call, but build up the command line using
 * sprintf() conventions.
 */
int
systemf(const char *fmt, ...)
{
	char buff[8192];
	va_list ap;
	int r;

	va_start(ap, fmt);
	vsprintf(buff, fmt, ap);
	if (verbose > 1)
		printf("Cmd: %s\n", buff);
	r = system(buff);
	va_end(ap);
	return (r);
}

/*
 * Slurp a file into memory for ease of comparison and testing.
 * Returns size of file in 'sizep' if non-NULL, null-terminates
 * data in memory for ease of use.
 */
char *
slurpfile(size_t * sizep, const char *fmt, ...)
{
	char filename[8192];
	struct stat st;
	va_list ap;
	char *p;
	ssize_t bytes_read;
	FILE *f;
	int r;

	va_start(ap, fmt);
	vsprintf(filename, fmt, ap);
	va_end(ap);

	f = fopen(filename, "rb");
	if (f == NULL) {
		/* Note: No error; non-existent file is okay here. */
		return (NULL);
	}
	r = fstat(fileno(f), &st);
	if (r != 0) {
		fprintf(stderr, "Can't stat file %s\n", filename);
		fclose(f);
		return (NULL);
	}
	p = malloc((size_t)st.st_size + 1);
	if (p == NULL) {
		fprintf(stderr, "Can't allocate %ld bytes of memory to read file %s\n", (long int)st.st_size, filename);
		fclose(f);
		return (NULL);
	}
	bytes_read = fread(p, 1, (size_t)st.st_size, f);
	if (bytes_read < st.st_size) {
		fprintf(stderr, "Can't read file %s\n", filename);
		fclose(f);
		free(p);
		return (NULL);
	}
	p[st.st_size] = '\0';
	if (sizep != NULL)
		*sizep = (size_t)st.st_size;
	fclose(f);
	return (p);
}

/*
 * "list.h" is automatically generated; it just has a lot of lines like:
 * 	DEFINE_TEST(function_name)
 * It's used above to declare all of the test functions.
 * We reuse it here to define a list of all tests (functions and names).
 */
#undef DEFINE_TEST
#define	DEFINE_TEST(n) { n, #n },
struct { void (*func)(void); const char *name; } tests[] = {
	#include "list.h"
};

/*
 * Each test is run in a private work dir.  Those work dirs
 * do have consistent and predictable names, in case a group
 * of tests need to collaborate.  However, there is no provision
 * for requiring that tests run in a certain order.
 */
static int test_run(int i, const char *tmpdir)
{
	int failures_before = failures;
	int oldumask;

	if (!quiet_flag) {
		printf("%d: %s\n", i, tests[i].name);
		fflush(stdout);
	}

	/*
	 * Always explicitly chdir() in case the last test moved us to
	 * a strange place.
	 */
	if (!assertChdir(tmpdir)) {
		fprintf(stderr,
		    "ERROR: Couldn't chdir to temp dir %s\n",
		    tmpdir);
		exit(1);
	}
	/* Create a temp directory for this specific test. */
	if (!assertMakeDir(tests[i].name, 0755)) {
		fprintf(stderr,
		    "ERROR: Couldn't create temp dir ``%s''\n",
		    tests[i].name);
		exit(1);
	}
	/* Chdir() to that work directory. */
	if (!assertChdir(tests[i].name)) {
		fprintf(stderr,
		    "ERROR: Couldn't chdir to temp dir ``%s''\n",
		    tests[i].name);
		exit(1);
	}
	/* Explicitly reset the locale before each test. */
	setlocale(LC_ALL, "C");
	/* Record the umask before we run the test. */
	umask(oldumask = umask(0));
	/* Run the actual test. */
	(*tests[i].func)();
	/* Restore umask */
	umask(oldumask);
	/* Summarize the results of this test. */
	summarize();
	/* If there were no failures, we can remove the work dir. */
	if (failures == failures_before) {
		if (!keep_temp_files && assertChdir(tmpdir)) {
#if defined(_WIN32) && !defined(__CYGWIN__)
			systemf("rmdir /S /Q %s", tests[i].name);
#else
			systemf("rm -rf %s", tests[i].name);
#endif
		}
	}
	/* Return appropriate status. */
	return (failures == failures_before ? 0 : 1);
}

static void usage(const char *program)
{
	static const int limit = sizeof(tests) / sizeof(tests[0]);
	int i;

	printf("Usage: %s [options] <test> <test> ...\n", program);
	printf("Default is to run all tests.\n");
	printf("Otherwise, specify the numbers of the tests you wish to run.\n");
	printf("Options:\n");
	printf("  -d  Dump core after any failure, for debugging.\n");
	printf("  -k  Keep all temp files.\n");
	printf("      Default: temp files for successful tests deleted.\n");
#ifdef PROGRAM
	printf("  -p <path>  Path to executable to be tested.\n");
	printf("      Default: path taken from " ENVBASE " environment variable.\n");
#endif
	printf("  -q  Quiet.\n");
	printf("  -r <dir>   Path to dir containing reference files.\n");
	printf("      Default: Current directory.\n");
	printf("  -v  Verbose.\n");
	printf("Available tests:\n");
	for (i = 0; i < limit; i++)
		printf("  %d: %s\n", i, tests[i].name);
	exit(1);
}

#define	UUDECODE(c) (((c) - 0x20) & 0x3f)

void
extract_reference_file(const char *name)
{
	char buff[1024];
	FILE *in, *out;

	sprintf(buff, "%s/%s.uu", refdir, name);
	in = fopen(buff, "r");
	failure("Couldn't open reference file %s", buff);
	assert(in != NULL);
	if (in == NULL)
		return;
	/* Read up to and including the 'begin' line. */
	for (;;) {
		if (fgets(buff, sizeof(buff), in) == NULL) {
			/* TODO: This is a failure. */
			return;
		}
		if (memcmp(buff, "begin ", 6) == 0)
			break;
	}
	/* Now, decode the rest and write it. */
	/* Not a lot of error checking here; the input better be right. */
	out = fopen(name, "wb");
	while (fgets(buff, sizeof(buff), in) != NULL) {
		char *p = buff;
		int bytes;

		if (memcmp(buff, "end", 3) == 0)
			break;

		bytes = UUDECODE(*p++);
		while (bytes > 0) {
			int n = 0;
			/* Write out 1-3 bytes from that. */
			if (bytes > 0) {
				n = UUDECODE(*p++) << 18;
				n |= UUDECODE(*p++) << 12;
				fputc(n >> 16, out);
				--bytes;
			}
			if (bytes > 0) {
				n |= UUDECODE(*p++) << 6;
				fputc((n >> 8) & 0xFF, out);
				--bytes;
			}
			if (bytes > 0) {
				n |= UUDECODE(*p++);
				fputc(n & 0xFF, out);
				--bytes;
			}
		}
	}
	fclose(out);
	fclose(in);
}

static char *
get_refdir(const char *d)
{
	char tried[512] = { '\0' };
	char buff[128];
	char *pwd, *p;

	/* If a dir was specified, try that */
	if (d != NULL) {
		pwd = NULL;
		strcpy(buff, d);
		p = slurpfile(NULL, "%s/%s", buff, KNOWNREF);
		if (p != NULL) goto success;
		strcat(tried, d);
		goto failure;
	}

	/* Get the current dir. */
	pwd = getcwd(NULL, 0);
	while (pwd[strlen(pwd) - 1] == '\n')
		pwd[strlen(pwd) - 1] = '\0';
	printf("PWD: %s\n", pwd);

	/* Look for a known file. */
	snprintf(buff, sizeof(buff), "%s", pwd);
	p = slurpfile(NULL, "%s/%s", buff, KNOWNREF);
	if (p != NULL) goto success;
	strncat(tried, buff, sizeof(tried) - strlen(tried) - 1);
	strncat(tried, "\n", sizeof(tried) - strlen(tried) - 1);

	snprintf(buff, sizeof(buff), "%s/test", pwd);
	p = slurpfile(NULL, "%s/%s", buff, KNOWNREF);
	if (p != NULL) goto success;
	strncat(tried, buff, sizeof(tried) - strlen(tried) - 1);
	strncat(tried, "\n", sizeof(tried) - strlen(tried) - 1);

#if defined(LIBRARY)
	snprintf(buff, sizeof(buff), "%s/%s/test", pwd, LIBRARY);
#else
	snprintf(buff, sizeof(buff), "%s/%s/test", pwd, PROGRAM);
#endif
	p = slurpfile(NULL, "%s/%s", buff, KNOWNREF);
	if (p != NULL) goto success;
	strncat(tried, buff, sizeof(tried) - strlen(tried) - 1);
	strncat(tried, "\n", sizeof(tried) - strlen(tried) - 1);

	if (memcmp(pwd, "/usr/obj", 8) == 0) {
		snprintf(buff, sizeof(buff), "%s", pwd + 8);
		p = slurpfile(NULL, "%s/%s", buff, KNOWNREF);
		if (p != NULL) goto success;
		strncat(tried, buff, sizeof(tried) - strlen(tried) - 1);
		strncat(tried, "\n", sizeof(tried) - strlen(tried) - 1);

		snprintf(buff, sizeof(buff), "%s/test", pwd + 8);
		p = slurpfile(NULL, "%s/%s", buff, KNOWNREF);
		if (p != NULL) goto success;
		strncat(tried, buff, sizeof(tried) - strlen(tried) - 1);
		strncat(tried, "\n", sizeof(tried) - strlen(tried) - 1);
	}

failure:
	printf("Unable to locate known reference file %s\n", KNOWNREF);
	printf("  Checked following directories:\n%s\n", tried);
#if defined(_WIN32) && !defined(__CYGWIN__) && defined(_DEBUG)
	DebugBreak();
#endif
	exit(1);

success:
	free(p);
	free(pwd);
	return strdup(buff);
}

int main(int argc, char **argv)
{
	static const int limit = sizeof(tests) / sizeof(tests[0]);
	int i, tests_run = 0, tests_failed = 0, option;
	time_t now;
	char *refdir_alloc = NULL;
	const char *progname;
	const char *tmp, *option_arg, *p;
	char tmpdir[256];
	char tmpdir_timestamp[256];

	(void)argc; /* UNUSED */

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__GNUC__)
	/* To stop to run the default invalid parameter handler. */
	_set_invalid_parameter_handler(invalid_parameter_handler);
	/* Disable annoying assertion message box. */
	_CrtSetReportMode(_CRT_ASSERT, 0);
#endif

	/*
	 * Name of this program, used to build root of our temp directory
	 * tree.
	 */
	progname = p = argv[0];
	while (*p != '\0') {
		/* Support \ or / dir separators for Windows compat. */
		if (*p == '/' || *p == '\\')
			progname = p + 1;
		++p;
	}

#ifdef PROGRAM
	/* Get the target program from environment, if available. */
	testprogfile = getenv(ENVBASE);
#endif

	if (getenv("TMPDIR") != NULL)
		tmp = getenv("TMPDIR");
	else if (getenv("TMP") != NULL)
		tmp = getenv("TMP");
	else if (getenv("TEMP") != NULL)
		tmp = getenv("TEMP");
	else if (getenv("TEMPDIR") != NULL)
		tmp = getenv("TEMPDIR");
	else
		tmp = "/tmp";

	/* Allow -d to be controlled through the environment. */
	if (getenv(ENVBASE "_DEBUG") != NULL)
		dump_on_failure = 1;

	/* Get the directory holding test files from environment. */
	refdir = getenv(ENVBASE "_TEST_FILES");

	/*
	 * Parse options, without using getopt(), which isn't available
	 * on all platforms.
	 */
	++argv; /* Skip program name */
	while (*argv != NULL) {
		if (**argv != '-')
			break;
		p = *argv++;
		++p; /* Skip '-' */
		while (*p != '\0') {
			option = *p++;
			option_arg = NULL;
			/* If 'opt' takes an argument, parse that. */
			if (option == 'p' || option == 'r') {
				if (*p != '\0')
					option_arg = p;
				else if (*argv == NULL) {
					fprintf(stderr,
					    "Option -%c requires argument.\n",
					    option);
					usage(progname);
				} else
					option_arg = *argv++;
				p = ""; /* End of this option word. */
			}

			/* Now, handle the option. */
			switch (option) {
			case 'd':
				dump_on_failure = 1;
				break;
			case 'k':
				keep_temp_files = 1;
				break;
			case 'p':
#ifdef PROGRAM
				testprogfile = option_arg;
#else
				usage(progname);
#endif
				break;
			case 'q':
				quiet_flag++;
				break;
			case 'r':
				refdir = option_arg;
				break;
			case 'v':
				verbose ++;
				break;
			default:
				usage(progname);
			}
		}
	}

	/*
	 * Sanity-check that our options make sense.
	 */
#ifdef PROGRAM
	if (testprogfile == NULL)
		usage(progname);
	{
		char *testprg;
#if defined(_WIN32) && !defined(__CYGWIN__)
		/* Command.com sometimes rejects '/' separators. */
		testprg = strdup(testprogfile);
		for (i = 0; testprg[i] != '\0'; i++) {
			if (testprg[i] == '/')
				testprg[i] = '\\';
		}
		testprogfile = testprg;
#endif
		/* Quote the name that gets put into shell command lines. */
		testprg = malloc(strlen(testprogfile) + 3);
		strcpy(testprg, "\"");
		strcat(testprg, testprogfile);
		strcat(testprg, "\"");
		testprog = testprg;
	}
#endif

	/*
	 * Create a temp directory for the following tests.
	 * Include the time the tests started as part of the name,
	 * to make it easier to track the results of multiple tests.
	 */
	now = time(NULL);
	for (i = 0; ; i++) {
		strftime(tmpdir_timestamp, sizeof(tmpdir_timestamp),
		    "%Y-%m-%dT%H.%M.%S",
		    localtime(&now));
		sprintf(tmpdir, "%s/%s.%s-%03d", tmp, progname,
		    tmpdir_timestamp, i);
		if (assertMakeDir(tmpdir,0755))
			break;
		if (i >= 999) {
			fprintf(stderr,
			    "ERROR: Unable to create temp directory %s\n",
			    tmpdir);
			exit(1);
		}
	}

	/*
	 * If the user didn't specify a directory for locating
	 * reference files, try to find the reference files in
	 * the "usual places."
	 */
	refdir = refdir_alloc = get_refdir(refdir);

	/*
	 * Banner with basic information.
	 */
	if (!quiet_flag) {
		printf("Running tests in: %s\n", tmpdir);
		printf("Reference files will be read from: %s\n", refdir);
#ifdef PROGRAM
		printf("Running tests on: %s\n", testprog);
#endif
		printf("Exercising: ");
		fflush(stdout);
		printf("%s\n", EXTRA_VERSION);
	}

	/*
	 * Run some or all of the individual tests.
	 */
	if (*argv == NULL) {
		/* Default: Run all tests. */
		for (i = 0; i < limit; i++) {
			if (test_run(i, tmpdir))
				tests_failed++;
			tests_run++;
		}
	} else {
		while (*(argv) != NULL) {
			if (**argv >= '0' && **argv <= '9') {
				i = atoi(*argv);
				if (i < 0 || i >= limit) {
					printf("*** INVALID Test %s\n", *argv);
					free(refdir_alloc);
					usage(progname);
					/* usage() never returns */
				}
			} else {
				for (i = 0; i < limit; ++i) {
					if (strcmp(*argv, tests[i].name) == 0)
						break;
				}
				if (i >= limit) {
					printf("*** INVALID Test ``%s''\n",
					       *argv);
					free(refdir_alloc);
					usage(progname);
					/* usage() never returns */
				}
			}
			if (test_run(i, tmpdir))
				tests_failed++;
			tests_run++;
			argv++;
		}
	}

	/*
	 * Report summary statistics.
	 */
	if (!quiet_flag) {
		printf("\n");
		printf("%d of %d tests reported failures\n",
		    tests_failed, tests_run);
		printf(" Total of %d assertions checked.\n", assertions);
		printf(" Total of %d assertions failed.\n", failures);
		printf(" Total of %d reported skips.\n", skips);
	}

	free(refdir_alloc);

	/* If the final tmpdir is empty, we can remove it. */
	/* This should be the usual case when all tests succeed. */
	assertChdir("..");
	rmdir(tmpdir);

	return (tests_failed);
}
