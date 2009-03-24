/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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
 *
 * $FreeBSD$
 */

#if defined(_WIN32) && !defined(__CYGWIN__)
#define _WIN32_WINNT 0x0500
#define WINVER       0x0500

#include "bsdtar_platform.h"
#include <errno.h>
#include <stddef.h>
#include <sys/utime.h>
#include <sys/stat.h>
#include <process.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <sddl.h>

#include "bsdtar.h"

int
bsdtar_is_privileged(struct bsdtar *bsdtar)
{
	int ret;

	ret = la_is_privileged();
	if (ret < 0) {
		bsdtar_warnc(bsdtar, errno, "Failed to check privilege");
		return (0);
	}
	return (ret);
}

/*
 * Note: We should use wide-character for findng '\' character,
 * a directory separator on Windows, because some character-set have
 * been using the '\' character for a part of its multibyte character
 * code.
 */
static size_t
dir_len_w(const char *path)
{
	wchar_t wc;
	const char *p, *rp;
	size_t al, l, size;

	al = l = -1;
	for (p = path; *p != '\0'; ++p) {
		if (*p == '\\')
			al = l = p - path;
		else if (*p == '/')
			al = p - path;
	}
	if (l == -1)
		goto alen;
	size = p - path;
	rp = p = path;
	while (*p != '\0') {
		l = mbtowc(&wc, p, size);
		if (l == -1)
			goto alen;
		if (l == 1 && (wc == L'/' || wc == L'\\'))
			rp = p;
		p += l;
		size -= l;
	}
	return (rp - path + 1);
alen:
	if (al == -1)
		return (0);
	return (al + 1);
}

/*
 * Find file names and call write_hierarchy function.
 */
void
write_hierarchy_win(struct bsdtar *bsdtar, struct archive *a,
    const char *path, void (*write_hierarchy)(struct bsdtar *bsdtar,
    struct archive *a, const char *path))
{
	DIR *dir;
	struct dirent *ent;
	const char *r;
	char *xpath;
	size_t dl;

	r = path;
	while (*r != '\0' && *r != '*' && *r != '?')
		++r;
	if (*r == '\0')
		/* There aren't meta-characters '*' and '?' in path */
		goto try_plain;
	dir = opendir_findfile(path);
	if (dir == NULL)
		goto try_plain;
	dl = dir_len_w(path);
	xpath = malloc(dl + MAX_PATH);
	if (xpath == NULL)
		goto try_plain;
	strncpy(xpath, path, dl);
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.' && ent->d_name[1] == '\0')
			continue;
		if (ent->d_name[0] == '.' && ent->d_name[1] == '.' &&
		    ent->d_name[2] == '\0')
			continue;
		strcpy(&xpath[dl], ent->d_name);
		write_hierarchy(bsdtar, a, xpath);
	}
	free(xpath);
	closedir(dir);
	return;

try_plain:
	write_hierarchy(bsdtar, a, path);
}

#endif
