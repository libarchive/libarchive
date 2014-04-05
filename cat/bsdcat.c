/*-
 * Copyright (c) 2011-2014, Mike Kazantsev
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bsdcat.h"
#include "err.h"

#define	BYTES_PER_BLOCK	(20*512)

struct archive *a;
struct archive_entry *ae;
char *bsdcat_current_path;


void
usage(void)
{
	const char *p;
	p = lafe_getprogname();
	fprintf(stderr, "Usage: %s [-h] [--help] [--version] [--] [filenames...]\n", p);
	exit(1);
}

static void
version(void)
{
	printf("bsdcat %s - %s\n",
	    BSDCAT_VERSION_STRING,
	    archive_version_details());
	exit(0);
}

void
bsdcat_next()
{
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_raw(a);
}

void
bsdcat_print_error(void)
{
	lafe_warnc(0, "%s: %s",
	    bsdcat_current_path, archive_error_string(a));
}

void
bsdcat_read_to_stdout(char* filename)
{
	if ((archive_read_open_filename(a, filename, BYTES_PER_BLOCK) != ARCHIVE_OK)
	    || (archive_read_next_header(a, &ae) != ARCHIVE_OK)
	    || (archive_read_data_into_fd(a, 1) != ARCHIVE_OK))
		bsdcat_print_error();
	if (archive_read_free(a) != ARCHIVE_OK)
		bsdcat_print_error();
}

int
main(int argc, char **argv)
{
	int c;

	lafe_setprogname(*argv, "bsdcat");

	while ((c = getopt(argc, argv, "h-")) != -1) {
		switch (c) {
			case '-':
				if (strcmp(argv[optind], "--version") == 0) version();
				if (c == '-' && strcmp(argv[optind], "--help") != 0)
					lafe_warnc(0, "invalid option -- '%s'", argv[optind]);
			default:
				usage();
		}
	}

	bsdcat_next();
	if (optind >= argc) {
		bsdcat_current_path = "<stdin>";
		bsdcat_read_to_stdout(NULL);
	} else
		while (optind < argc) {
			bsdcat_current_path = argv[optind++];
			bsdcat_read_to_stdout(bsdcat_current_path);
			bsdcat_next();
		}

	exit(0);
}
