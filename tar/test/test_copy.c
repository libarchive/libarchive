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

/* assumes that cwd is the top of the test tree. Furthermore,
 * assumes that this function is first called with the "longest"
 * cwd involved in the tests.  That is, from
 *  <testdir>/original
 * as opposed to
 *  <testdir>/plain or <testdir>/ustar
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
		/* on windows, can't create a directory in which there is not
		 * enough room left in MAX_PATH to /also/ create an 8.3 file.
		 * Thus, max path len for mkdir is MAX_PATH - 12 ("12345678.123")
		 * It is possible also that windows counts the length of cwd against
		 * the MAX_PATH maximum, so account for that. Next, account also for
		 * "/".  And lastly, account for the fact that the relative path
		 * has 4 characters when the loop count i = 0.
		 */
		 LOOP_MAX = MAX_PATH - 12 - (int)cwdlen - 1 - 4;
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
		/* on windows, can't create a directory in which there is not
		 * enough room left in PATH_MAX to /also/ create an 8.3 file.
		 * Thus, max path len for mkdir is PATH_MAX - 12 ("12345678.123")
		 * Then, because cygwin treats even relative paths as if they were
		 * absolute, and cwd counts against the PATH_MAX maximum, we must
		 * account for that (using worst case of posix or win32 equivalents).
		 * Next, account also for "/../" (as used in symlink creation test).
		 * And lastly, account for the fact that the relative path has 4
		 * characters when the loop count i = 0. These calculations do not
		 * apply to cygwin-1.7, because unlike older cygwin, it uses the "wide"
		 * functions of the win32 system for all file and directory access.
		 */
		LOOP_MAX = PATH_MAX - 12 - (int)cwdlen - 4 - 4;
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

	assertEqualInt(0, mkdir("original", 0775));
	chdir("original");
	LOOP_MAX = compute_loop_max();

	assertEqualInt(0, mkdir("f", 0775));
	assertEqualInt(0, mkdir("l", 0775));
	assertEqualInt(0, mkdir("m", 0775));
	assertEqualInt(0, mkdir("s", 0775));
	assertEqualInt(0, mkdir("d", 0775));

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
		assertEqualInt(0, link(buff, buff2));

		/* Create a link named "m/abcdef..." to the above. */
		strcpy(buff2, buff);
		buff2[0] = 'm';
		assertEqualInt(0, link(buff, buff2));

#if !defined(_WIN32) || defined(__CYGWIN__)
		/* Create a symlink named "s/abcdef..." to the above. */
		strcpy(buff2 + 3, buff);
		buff[0] = 's';
		buff2[0] = '.';
		buff2[1] = '.';
		buff2[2] = '/';
		failure("buff=\"%s\" buff2=\"%s\"", buff, buff2);
		assertEqualInt(0, symlink(buff2, buff));
#else
		skipping("create a symlink to the above");
#endif
		/* Create a dir named "d/abcdef...". */
		buff[0] = 'd';
		failure("buff=\"%s\"", buff);
		assertEqualInt(0, mkdir(buff, 0775));
	}

	chdir("..");
}

#define LIMIT_NONE 0
#define LIMIT_USTAR 1

static void
verify_tree(int limit)
{
	struct stat st, st2;
	char filename[260];
	char name1[260];
	char name2[260];
	char contents[260];
	int i, j, r, LOOP_MAX;
	FILE *f;
	int len;
	const char *p, *dp;
	DIR *d;
	struct dirent *de;

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
			f = fopen(name1, "rb");
			failure("Couldn't open \"%s\": %s",
			    name1, strerror(errno));
			if (assert(f != NULL)) {
				len = fread(contents, 1, i + 10, f);
				fclose(f);
				assertEqualInt(len, i + 2);
				/* Verify contents of 'contents' */
				contents[len] = '\0';
				failure("Each test file contains its own name");
				assertEqualString(name1, contents);
				/* stat() for dev/ino for next check */
				assertEqualInt(0, lstat(name1, &st));
			}
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
			assertEqualInt(0, (r = lstat(name2, &st2)));
			if (r == 0) {
				assertEqualInt(st2.st_dev, st.st_dev);
				assertEqualInt(st2.st_ino, st.st_ino);
			}

			/* Verify hardlink "m_abcdef..." */
			name2[0] = 'm';
			assertEqualInt(0, (r = lstat(name2, &st2)));
			if (r == 0) {
				assertEqualInt(st2.st_dev, st.st_dev);
				assertEqualInt(st2.st_ino, st.st_ino);
			}
		}

#if !defined(_WIN32) || defined(__CYGWIN__)
		/*
		 * Symlink text doesn't include the 'original/' prefix,
		 * so the limit here is 100 characters.
		 */
		/* Verify symlink "s/abcdef..." */
		strcpy(name2, "../s/");
		strcat(name2, filename);
		if (limit != LIMIT_USTAR || strlen(name2) <= 100) {
			/* This is a symlink. */
			failure("Couldn't stat %s (length %d)",
			    filename, strlen(filename));
			if (assertEqualInt(0, lstat(name2 + 3, &st2))) {
				assert(S_ISLNK(st2.st_mode));
				/* This is a symlink to the file above. */
				failure("Couldn't stat %s", name2 + 3);
				if (assertEqualInt(0, stat(name2 + 3, &st2))) {
					assertEqualInt(st2.st_dev, st.st_dev);
					assertEqualInt(st2.st_ino, st.st_ino);
				}
			}
		}
#else
		skipping("verify symlink");
#endif
		/* Verify dir "d/abcdef...". */
		strcpy(name1, "d/");
		strcat(name1, filename);
		if (limit != LIMIT_USTAR || strlen(filename) < 100) {
			/* This is a dir. */
			failure("Couldn't stat %s (length %d)",
			    name1, strlen(filename));
			if (assertEqualInt(0, lstat(name1, &st2))) {
				if (assert(S_ISDIR(st2.st_mode))) {
					/* TODO: opendir/readdir this
					 * directory and make sure
					 * it's empty.
					 */
				}
			}
		}
	}

	/* Now make sure nothing is there that shouldn't be. */
	for (dp = "dflms"; *dp != '\0'; ++dp) {
		char dir[2];
		dir[0] = *dp; dir[1] = '\0';
		d = opendir(dir);
		failure("Unable to open dir '%s'", dir);
		if (!assert(d != NULL))
			continue;
		while ((de = readdir(d)) != NULL) {
			p = de->d_name;
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
	assertEqualInt(0, mkdir("plain", 0775));
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
	assertEqualInt(0, mkdir(target, 0775));
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
	int oldumask;

	oldumask = umask(0);

	create_tree(); /* Create sample files in "original" dir. */

	/* Test simple "tar -c | tar -x" pipeline copy. */
	copy_basic();

	/* Same, but constrain to ustar format. */
	copy_ustar();

	umask(oldumask);
}
