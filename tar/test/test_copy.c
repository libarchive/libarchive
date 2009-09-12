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
#include "test.h"
__FBSDID("$FreeBSD: src/usr.bin/tar/test/test_copy.c,v 1.3 2008/08/15 06:12:02 kientzle Exp $");

#if defined(__CYGWIN__)
# include <limits.h>
# include <sys/cygwin.h>
#endif

/*
 * Try to figure out how deep we can go in our tests.  Assumes that
 * the first call to this function has the longest starting cwd (which
 * is currently "<testdir>/original").  This is mostly to work around
 * limits in our Win32 support.
 *
 * Background: On Posix systems, PATH_MAX is merely a limit on the
 * length of the string passed into a system call.  By repeatedly
 * calling chdir(), you can work with arbitrarily long paths on such
 * systems.  In contrast, Win32 APIs apply PATH_MAX limits to the full
 * absolute path, so the permissible length of a system call argument
 * varies with the cwd. Some APIs actually enforce limits
 * significantly less than PATH_MAX to ensure that you can create
 * files within the current working directory.  The Win32 limits also
 * apply to Cygwin before 1.7.
 *
 * Someday, I want to convert the Win32 support to use newer
 * wide-character paths with '\\?\' prefix, which has a 32k PATH_MAX
 * instead of the rather anemic 260 character limit of the older
 * system calls.  Then we can drop this mess (unless we want to
 * continue to special-case Cygwin 1.5 and earlier).
 */
static int
compute_loop_max(void)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	static int LOOP_MAX = 0;
	char buf[MAX_PATH];
	size_t cwdlen;

	if (LOOP_MAX == 0) {
		assert(_getcwd(buf, MAX_PATH) != NULL);
		cwdlen = strlen(buf);
		/* 12 characters = length of 8.3 filename */
		/* 4 characters = length of "/../" used in symlink tests */
		/* 1 character = length of extra "/" separator */
		LOOP_MAX = MAX_PATH - (int)cwdlen - 12 - 4 - 1;
	}
	return LOOP_MAX;
#elif defined(__CYGWIN__) && !defined(HAVE_CYGWIN_CONV_PATH)
	static int LOOP_MAX = 0;
	if (LOOP_MAX == 0) {
		char wbuf[PATH_MAX];
		char pbuf[PATH_MAX];
		size_t wcwdlen;
		size_t pcwdlen;
	        size_t cwdlen;
		assert(getcwd(pbuf, PATH_MAX) != NULL);
		pcwdlen = strlen(pbuf);
		cygwin_conv_to_full_win32_path(pbuf, wbuf);
		wcwdlen = strlen(wbuf);
		cwdlen = ((wcwdlen > pcwdlen) ? wcwdlen : pcwdlen);
		/* Cygwin helper needs an extra few characters. */
		LOOP_MAX = PATH_MAX - (int)cwdlen - 12 - 4 - 4;
	}
	return LOOP_MAX;
#else
	/* cygwin-1.7 ends up here, along with "normal" unix */
	return 200; /* restore pre-r278 depth */
#endif
}

static void
create_tree(void)
{
	char buff[260];
	char buff2[260];
	int i;
	FILE *f;
	int LOOP_MAX;

	assertMakeDir("original", 0775);
	chdir("original");
	LOOP_MAX = compute_loop_max();

	assertMakeDir("f", 0775);
	assertMakeDir("l", 0775);
	assertMakeDir("m", 0775);
	assertMakeDir("s", 0775);
	assertMakeDir("d", 0775);

	for (i = 0; i < LOOP_MAX; i++) {
		buff[0] = 'f';
		buff[1] = '/';
		/* Create a file named "f/abcdef..." */
		buff[i + 2] = 'a' + (i % 26);
		buff[i + 3] = '\0';
		f = fopen(buff, "w+");
		failure("f = fopen(\"%s\", \"w+\")", buff);
		assert(f != NULL);
		fprintf(f, buff);
		fclose(f);

		/* Create a link named "l/abcdef..." to the above. */
		strcpy(buff2, buff);
		buff2[0] = 'l';
		assertMakeHardlink(buff2, buff);

		/* Create a link named "m/abcdef..." to the above. */
		strcpy(buff2, buff);
		buff2[0] = 'm';
		assertMakeHardlink(buff2, buff);

		if (canSymlink()) {
			/* Create a symlink named "s/abcdef..." to the above. */
			strcpy(buff2 + 3, buff);
			buff[0] = 's';
			buff2[0] = '.';
			buff2[1] = '.';
			buff2[2] = '/';
			failure("buff=\"%s\" buff2=\"%s\"", buff, buff2);
			assertMakeSymlink(buff, buff2);
		} else {
			skipping("Symlink tests");
		}
		/* Create a dir named "d/abcdef...". */
		buff[0] = 'd';
		failure("buff=\"%s\"", buff);
		assertMakeDir(buff, 0775);
	}

	chdir("..");
}

#define LIMIT_NONE 0
#define LIMIT_USTAR 1

static void
verify_tree(int limit)
{
	char filename[260];
	char name1[260];
	char name2[260];
	int i, j, LOOP_MAX;

	LOOP_MAX = compute_loop_max();

	/* Generate the names we know should be there and verify them. */
	for (i = 1; i < LOOP_MAX; i++) {
		/* Generate a base name of the correct length. */
		for (j = 0; j < i; ++j)
			filename[j] = 'a' + (j % 26);
#if 0
		for (n = i; n > 0; n /= 10)
			filename[--j] = '0' + (n % 10);
#endif
		filename[i] = '\0';

		/* Verify a file named "f/abcdef..." */
		strcpy(name1, "f/");
		strcat(name1, filename);
		if (limit != LIMIT_USTAR || strlen(filename) <= 100) {
			assertFileExists(name1);
			assertFileContents(name1, strlen(name1), name1);
		}

		/*
		 * ustar allows 100 chars for links, and we have
		 * "original/" as part of the name, so the link
		 * names here can't exceed 91 chars.
		 */
		strcpy(name2, "l/");
		strcat(name2, filename);
		if (limit != LIMIT_USTAR || strlen(name2) <= 100) {
			/* Verify hardlink "l/abcdef..." */
			assertIsHardlink(name1, name2);
			/* Verify hardlink "m/abcdef..." */
			name2[0] = 'm';
			assertIsHardlink(name1, name2);
		}

		if (canSymlink()) {
			/* Verify symlink "s/abcdef..." */
			strcpy(name1, "s/");
			strcat(name1, filename);
			strcpy(name2, "../f/");
			strcat(name2, filename);
			if (limit != LIMIT_USTAR || strlen(name2) <= 100)
				assertIsSymlink(name1, name2);
		}

		/* Verify dir "d/abcdef...". */
		strcpy(name1, "d/");
		strcat(name1, filename);
		if (limit != LIMIT_USTAR || strlen(filename) < 100) {
			if (assertIsDir(name1, -1)) {
				/* TODO: opendir/readdir this
				 * directory and make sure
				 * it's empty.
				 */
			}
		}
	}

#if !defined(_WIN32) || defined(__CYGWIN__)
	{
		const char *dp;
		/* Now make sure nothing is there that shouldn't be. */
		for (dp = "dflms"; *dp != '\0'; ++dp) {
			DIR *d;
			struct dirent *de;
			char dir[2];
			dir[0] = *dp; dir[1] = '\0';
			d = opendir(dir);
			failure("Unable to open dir '%s'", dir);
			if (!assert(d != NULL))
				continue;
			while ((de = readdir(d)) != NULL) {
				char *p = de->d_name;
				switch(dp[0]) {
				case 'l': case 'm':
					if (limit == LIMIT_USTAR) {
						failure("strlen(p) = %d", strlen(p));
						assert(strlen(p) <= 100);
					}
				case 'd':
					if (limit == LIMIT_USTAR) {
						failure("strlen(p)=%d", strlen(p));
						assert(strlen(p) < 100);
					}
				case 'f': case 's':
					if (limit == LIMIT_USTAR) {
						failure("strlen(p)=%d", strlen(p));
						assert(strlen(p) < 101);
					}
					/* Our files have very particular filename patterns. */
					if (p[0] != '.' || (p[1] != '.' && p[1] != '\0')) {
						for (i = 0; p[i] != '\0' && i < LOOP_MAX; i++) {
							failure("i=%d, p[i]='%c' 'a'+(i%%26)='%c'", i, p[i], 'a' + (i % 26));
							assertEqualInt(p[i], 'a' + (i % 26));
						}
						assert(p[i] == '\0');
					}
					break;
				case '.':
					assert(p[1] == '\0' || (p[1] == '.' && p[2] == '\0'));
					break;
				default:
					failure("File %s shouldn't be here", p);
					assert(0);
				}
			}
			closedir(d);
		}
	}
#endif
}

static void
copy_basic(void)
{
	int r;

	/* NOTE: for proper operation on cygwin-1.5 and windows, the
	 * length of the name of the directory below, "plain", must be
	 * less than or equal to the lengthe of the name of the original
	 * directory, "original"  This restriction derives from the
	 * extremely limited pathname lengths on those platforms.
	 */
	assertMakeDir("plain", 0775);
	assertEqualInt(0, chdir("plain"));

	/*
	 * Use the tar program to create an archive.
	 */
	r = systemf("%s cf archive -C ../original f d l m s >pack.out 2>pack.err",
	    testprog);
	failure("Error invoking \"%s cf\"", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout or stderr. */
	assertEmptyFile("pack.err");
	assertEmptyFile("pack.out");

	/*
	 * Use tar to unpack the archive into another directory.
	 */
	r = systemf("%s xf archive >unpack.out 2>unpack.err", testprog);
	failure("Error invoking %s xf archive", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout or stderr. */
	assertEmptyFile("unpack.err");
	assertEmptyFile("unpack.out");

	verify_tree(LIMIT_NONE);
	assertEqualInt(0, chdir(".."));
}

static void
copy_ustar(void)
{
	const char *target = "ustar";
	int r;

	/* NOTE: for proper operation on cygwin-1.5 and windows, the
	 * length of the name of the directory below, "ustar", must be
	 * less than or equal to the lengthe of the name of the original
	 * directory, "original"  This restriction derives from the
	 * extremely limited pathname lengths on those platforms.
	 */
	assertMakeDir(target, 0775);
	assertEqualInt(0, chdir(target));

	/*
	 * Use the tar program to create an archive.
	 */
	r = systemf("%s cf archive --format=ustar -C ../original f d l m s >pack.out 2>pack.err",
	    testprog);
	failure("Error invoking \"%s cf archive --format=ustar\"", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout. */
	assertEmptyFile("pack.out");
	/* Stderr is non-empty, since there are a bunch of files
	 * with filenames too long to archive. */

	/*
	 * Use tar to unpack the archive into another directory.
	 */
	r = systemf("%s xf archive >unpack.out 2>unpack.err", testprog);
	failure("Error invoking %s xf archive", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout or stderr. */
	assertEmptyFile("unpack.err");
	assertEmptyFile("unpack.out");

	chdir("original");
	verify_tree(LIMIT_USTAR);
	chdir("../..");
}

DEFINE_TEST(test_copy)
{
	assertUmask(0);

	create_tree(); /* Create sample files in "original" dir. */

	/* Test simple "tar -c | tar -x" pipeline copy. */
	copy_basic();

	/* Same, but constrain to ustar format. */
	copy_ustar();
}
