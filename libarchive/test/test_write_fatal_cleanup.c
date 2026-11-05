/*-
 * Copyright (c) 2026 The libarchive authors
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

/*
 * Once a write fails the writer enters ARCHIVE_STATE_FATAL and
 * archive_write_close() no longer runs the filter and format close
 * handlers.  archive_write_free() must still release compressor state
 * (zlib, liblzma, ...); otherwise it leaks, which LeakSanitizer reports.
 */

#define CHUNK	(256 * 1024)

static la_ssize_t
failing_write(struct archive *a, void *client_data, const void *buff,
    size_t length)
{
	(void)client_data;
	(void)buff;
	(void)length;
	archive_set_error(a, EIO, "simulated write failure");
	return (-1);
}

/* Incompressible data, so the compressor has to emit output. */
static char *
make_noise(void)
{
	char *buf = malloc(CHUNK);
	uint32_t seed = 12345;
	size_t i;

	if (buf == NULL)
		return (NULL);
	for (i = 0; i < CHUNK; i++) {
		seed = seed * 1103515245 + 12345;
		buf[i] = (char)(seed >> 16);
	}
	return (buf);
}

/*
 * Write one entry until the client write fails, then close and free.
 * The archive must already have its format and filters set.
 */
static void
write_until_failure(struct archive *a, const char *what)
{
	struct archive_entry *ae;
	char *buf;
	la_ssize_t r = 0;
	int i;

	assert((buf = make_noise()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open2(a, NULL, NULL, failing_write, NULL, NULL));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0644);
	archive_entry_set_size(ae, 64 * CHUNK);
	r = archive_write_header(a, ae);
	archive_entry_free(ae);

	for (i = 0; r >= 0 && i < 64; i++)
		r = archive_write_data(a, buf, CHUNK);
	failure("%s: the simulated write failure was never hit", what);
	assert(r < 0);
	failure("%s", what);
	assertEqualInt(ARCHIVE_FATAL, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	free(buf);
}

DEFINE_TEST(test_write_filter_fatal_cleanup)
{
	static const char *filters[] = {
		"gzip", "bzip2", "xz", "lzma", "lzip", NULL
	};
	struct archive *a;
	int i;

	for (i = 0; filters[i] != NULL; i++) {
		assert((a = archive_write_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_write_set_format_pax_restricted(a));
		/* ARCHIVE_WARN means an external program would be used. */
		if (archive_write_add_filter_by_name(a, filters[i])
		    != ARCHIVE_OK) {
			skipping("%s filter not supported by library",
			    filters[i]);
			assertEqualInt(ARCHIVE_OK, archive_write_free(a));
			continue;
		}
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_write_set_bytes_per_block(a, 1024));
		write_until_failure(a, filters[i]);
	}
}

DEFINE_TEST(test_write_format_zip_fatal_cleanup)
{
	static const char *methods[] = {
		"deflate", "bzip2", "xz", "lzma", "zstd", NULL
	};
	struct archive *a;
	int i;

	for (i = 0; methods[i] != NULL; i++) {
		assert((a = archive_write_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
		if (archive_write_set_format_option(a, "zip", "compression",
		    methods[i]) != ARCHIVE_OK) {
			skipping("zip %s compression not supported",
			    methods[i]);
			assertEqualInt(ARCHIVE_OK, archive_write_free(a));
			continue;
		}
		/*
		 * The zstd encoder always runs with worker threads and
		 * buffers a whole job before emitting output.  At the
		 * default level a job is larger than the data written
		 * below, so the write failure would only be hit by
		 * close.  The lowest level keeps the jobs small.
		 */
		assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_option(
		    a, "zip", "compression-level", "1"));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_write_set_bytes_per_block(a, 1024));
		write_until_failure(a, methods[i]);
	}
}
