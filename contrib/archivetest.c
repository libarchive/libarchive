/*-
 * Copyright (c) 2019 Martin Matuska
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
 * This utility tests parsing files by libarchive
 *
 * It may be used to reproduce failures in testcases discovered by OSS-Fuzz
 * https://github.com/google/oss-fuzz/blob/master/projects/libarchive
 */

#include <stdio.h>
#include <stdlib.h>
#include <archive.h>
#include <archive_entry.h>

const char *errnostr(int errno)
{
	char *estr;
	switch(errno) {
		case ARCHIVE_EOF:
			estr = "ARCHIVE_EOF";
		break;
		case ARCHIVE_OK:
			estr = "ARCHIVE_OK";
		break;
		case ARCHIVE_WARN:
			estr = "ARCHIVE_WARN";
		break;
		case ARCHIVE_RETRY:
			estr = "ARCHIVE_RETRY";
		break;
		case ARCHIVE_FAILED:
			estr = "ARCHIVE_FAILED";
		break;
		case ARCHIVE_FATAL:
			estr = "ARCHIVE_FATAL";
		break;
		default:
			estr = "Unknown";
		break;
	}
	return (estr);
}

int main(int argc, char *argv[])
{
	struct archive *a;
	struct archive_entry *entry;
	char *filename;
	const char *p;
	char buffer[4096];
	int c;
	int r = ARCHIVE_OK;
	int format_printed;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s filename\n", argv[0]);
		exit(1);
	}

	filename = argv[1];

	a = archive_read_new();

	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	r = archive_read_open_filename(a, filename, 4096);
	if (r != ARCHIVE_OK) {
		archive_read_free(a);
		fprintf(stderr, "Error opening filename: %s\n", filename);
		exit(ARCHIVE_FATAL);
	}

	format_printed = 0;
	c = 1;
	while (1) {
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_FATAL) {
			fprintf(stdout,
			    "Entry %d: fatal error reding header\n", c);
			break;
		}
		if (!format_printed) {
			fprintf(stdout, "Filter: %s\nFormat: %s\n",
			    archive_filter_name(a, 0), archive_format_name(a));
			format_printed = 1;
		}
		if (r == ARCHIVE_RETRY)
			continue;
		if (r == ARCHIVE_EOF)
			break;
		p = archive_entry_pathname(entry);
		if (p == NULL || p[0] == '\0')
			fprintf(stdout, "Entry %d: %s, ureadable pathname\n",
			    c, errnostr(r));
		else
			fprintf(stdout, "Entry %d: %s, pathname: %s\n", c,
			    errnostr(r), p);
		while ((r = archive_read_data(a, buffer, 4096) > 0))
		;
		if (r == ARCHIVE_FATAL) {
			fprintf(stderr, "Entry %d: fatal error reading data\n",
			    c);
			break;
		}
		c++;
	}
	archive_read_free(a);

	fprintf(stdout, "Last return code: %s (%d)\n", errnostr(r), r);
	if (r == ARCHIVE_EOF || r == ARCHIVE_OK)
		exit(0);
	exit(1);
}
