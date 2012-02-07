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

#include "lafe_platform.h"
__FBSDID("$FreeBSD: src/usr.bin/cpio/matching.c,v 1.2 2008/06/21 02:20:20 kientzle Exp $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "archive.h"
#include "err.h"
#include "line_reader.h"
#include "matching.h"

/*
 * Utility functions to manage exclusion/inclusion patterns
 */

int
lafe_exclude_from_file(struct archive *matching, const char *pathname)
{
	struct lafe_line_reader *lr;
	const char *p;
	int ret = 0;

	lr = lafe_line_reader(pathname, 0);
	while ((p = lafe_line_reader_next(lr)) != NULL) {
		ret = archive_match_exclude_pattern(matching, p);
		if (ret == ARCHIVE_FATAL)
			lafe_errc(1, errno, "Out of memory");
	}
	lafe_line_reader_free(lr);
	return (ret);
}

int
lafe_include_from_file(struct archive *matching, const char *pathname,
    int nullSeparator)
{
	struct lafe_line_reader *lr;
	const char *p;
	int ret = 0;

	lr = lafe_line_reader(pathname, nullSeparator);
	while ((p = lafe_line_reader_next(lr)) != NULL) {
		ret = archive_match_include_pattern(matching, p);
		if (ret == ARCHIVE_FATAL)
			lafe_errc(1, errno, "Out of memory");
	}
	lafe_line_reader_free(lr);
	return (ret);
}

