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

#ifndef BSDTAR_WINDOWS_H
#define BSDTAR_WINDOWS_H 1

#define PRId64 "I64"
#define exit		la_exit
#define geteuid()	0

struct __DIR;
typedef struct __DIR DIR;
struct direct {
	unsigned char	d_nameln;
	char		d_name[MAX_PATH];
};

#ifdef __cplusplus
extern "C" {
#endif

extern void	la_exit(int status);

extern DIR	*opendir(const char *path);
extern struct dirent *readdir(DIR *dirp);
extern int	closedir(DIR *dirp);

extern int	bsdtar_is_privileged(struct bsdtar *bsdtar);
extern void	write_hierarchy_win(struct bsdtar *bsdtar, struct archive *a,
		const char *path,
		void (*write_hierarchy)(struct bsdtar *bsdtar,
		struct archive *a, const char *path));

#ifdef __cplusplus
}
#endif

#endif /* BSDTAR_WINDOWS_H */
