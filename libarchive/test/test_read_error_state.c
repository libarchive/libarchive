/*-
 * Copyright (c) 2026 libarchive contributors
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
 * Cut every supported format at many lengths, and damage one byte at many
 * positions, then read the result entry by entry. Whenever an operation
 * returns WARN, FAILED or FATAL it has to leave a non-zero archive_errno()
 * and a non-NULL archive_error_string() behind.
 *
 * The archives are either written here or are reference files that other
 * tests already use. Each one is read through a seekable memory buffer and
 * through a client that cannot seek, because some readers (zip) take a
 * different path in each case.
 */

enum {
	FMT_USTAR, FMT_PAX, FMT_GNUTAR, FMT_V7TAR,
	FMT_CPIO_ODC, FMT_CPIO_NEWC, FMT_CPIO_BIN,
	FMT_AR_BSD, FMT_AR_SVR4,
	FMT_ZIP, FMT_7ZIP, FMT_XAR, FMT_ISO9660, FMT_MTREE, FMT_WARC,
	FMT_NONE
};

struct source {
	const char *name;
	int format;		/* Format to write, or FMT_NONE. */
	const char *reference;	/* Reference file to read instead. */
};

static const struct source sources[] = {
	{ "ustar", FMT_USTAR, NULL },
	{ "pax", FMT_PAX, NULL },
	{ "gnutar", FMT_GNUTAR, NULL },
	{ "v7tar", FMT_V7TAR, NULL },
	{ "cpio odc", FMT_CPIO_ODC, NULL },
	{ "cpio newc", FMT_CPIO_NEWC, NULL },
	{ "cpio bin", FMT_CPIO_BIN, NULL },
	{ "ar bsd", FMT_AR_BSD, NULL },
	{ "ar svr4", FMT_AR_SVR4, NULL },
	{ "zip", FMT_ZIP, NULL },
	{ "7zip", FMT_7ZIP, NULL },
	{ "xar", FMT_XAR, NULL },
	{ "iso9660", FMT_ISO9660, NULL },
	{ "mtree", FMT_MTREE, NULL },
	{ "warc", FMT_WARC, NULL },
	{ "ar", FMT_NONE, "test_read_format_ar.ar" },
	{ "cab", FMT_NONE, "test_read_format_cab_1.cab" },
	{ "cpio", FMT_NONE, "test_compat_cpio_1.cpio" },
	{ "cpio odc", FMT_NONE, "test_read_format_cpio_filename_cp866.cpio" },
	{ "lha", FMT_NONE, "test_read_format_lha_lh6.lzh" },
	{ "rar", FMT_NONE, "test_read_format_rar.rar" },
	{ "rar end block", FMT_NONE, "test_read_format_rar_endarc_huge.rar" },
	{ "rar ppmd", FMT_NONE, "test_read_format_rar_ppmd_use_after_free.rar" },
	{ "rar skip", FMT_NONE, "test_read_format_rar_seek_data_cursor0.rar" },
	{ "rar5", FMT_NONE, "test_read_format_rar5_stored.rar" },
	{ "rar5 dir", FMT_NONE, "test_read_format_rar5_dirdata.rar" },
	{ "rpm", FMT_NONE, "test_read_format_cpio_svr4_gzip_rpm.rpm" },
	{ "tar", FMT_NONE, "test_read_format_tar_V_negative_size.tar" },
	{ "tar sparse", FMT_NONE, "test_read_format_gtar_sparse_1_17_posix10.tar" },
	{ "zip encrypted", FMT_NONE, "test_read_format_zip_encryption_header.zip" },
	{ "zip eocd", FMT_NONE,
	    "test_read_format_zip_with_invalid_traditional_eocd.zip" },
};

static int
set_format(struct archive *a, int format)
{
	switch (format) {
	case FMT_USTAR: return (archive_write_set_format_ustar(a));
	case FMT_PAX: return (archive_write_set_format_pax(a));
	case FMT_GNUTAR: return (archive_write_set_format_gnutar(a));
	case FMT_V7TAR: return (archive_write_set_format_v7tar(a));
	case FMT_CPIO_ODC: return (archive_write_set_format_cpio_odc(a));
	case FMT_CPIO_NEWC: return (archive_write_set_format_cpio_newc(a));
	case FMT_CPIO_BIN: return (archive_write_set_format_cpio_bin(a));
	case FMT_AR_BSD: return (archive_write_set_format_ar_bsd(a));
	case FMT_AR_SVR4: return (archive_write_set_format_ar_svr4(a));
	case FMT_ZIP: return (archive_write_set_format_zip(a));
	case FMT_7ZIP: return (archive_write_set_format_7zip(a));
	case FMT_XAR: return (archive_write_set_format_xar(a));
	case FMT_ISO9660: return (archive_write_set_format_iso9660(a));
	case FMT_MTREE: return (archive_write_set_format_mtree(a));
	case FMT_WARC: return (archive_write_set_format_warc(a));
	}
	return (ARCHIVE_FATAL);
}

/*
 * Write a small archive in memory. An entry the format cannot hold (a
 * symlink in an ar archive, say) is left out. Returns NULL when this build
 * cannot write the format.
 */
static char *
write_sample(int format, size_t *size)
{
	static const char long_name[] =
	    "a_file_name_long_enough_to_need_an_extension_header_in_the_"
	    "formats_that_have_one_and_then_some_more_characters_to_pass_100";
	struct archive_entry *ae;
	struct archive *a;
	size_t buff_size = 1024 * 1024;
	char *buff = malloc(buff_size);
	la_ssize_t n;
	int i, r;

	assert(buff != NULL);
	assert((a = archive_write_new()) != NULL);
	if (set_format(a, format) != ARCHIVE_OK) {
		archive_write_free(a);
		free(buff);
		return (NULL);
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buff_size, size));

	assert((ae = archive_entry_new()) != NULL);
	for (i = 0; i < 4; i++) {
		archive_entry_clear(ae);
		archive_entry_set_mtime(ae, 1000000000, 0);
		switch (i) {
		case 0:
			archive_entry_copy_pathname(ae, "file1");
			archive_entry_set_mode(ae, AE_IFREG | 0644);
			archive_entry_set_size(ae, 11);
			break;
		case 1:
			archive_entry_copy_pathname(ae, "dir/");
			archive_entry_set_mode(ae, AE_IFDIR | 0755);
			break;
		case 2:
			archive_entry_copy_pathname(ae, "link1");
			archive_entry_set_mode(ae, AE_IFLNK | 0777);
			archive_entry_copy_symlink(ae, "file1");
			break;
		default:
			archive_entry_copy_pathname(ae, long_name);
			archive_entry_set_mode(ae, AE_IFREG | 0644);
			archive_entry_set_size(ae, 10);
			break;
		}
		r = archive_write_header(a, ae);
		if (r < ARCHIVE_WARN)
			continue;
		if (archive_entry_size(ae) == 0)
			continue;
		/* A format may warn about a name it cannot hold. */
		n = archive_write_data(a, "hello world",
		    (size_t)archive_entry_size(ae));
		if (r == ARCHIVE_OK)
			assertEqualInt((int)archive_entry_size(ae), (int)n);
	}
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	return (buff);
}

/* A client that hands out the buffer in pieces and cannot seek. */
struct stream {
	const char *data;
	size_t size;
	size_t offset;
};

static la_ssize_t
read_stream(struct archive *a, void *client_data, const void **buffer)
{
	struct stream *stream = client_data;
	size_t available = stream->size - stream->offset;

	(void)a;
	if (available > 1000)
		available = 1000;
	*buffer = stream->data + stream->offset;
	stream->offset += available;
	return ((la_ssize_t)available);
}

static int
check_error(struct archive *a, const char *context, const char *operation,
    int r)
{
	int failed = 0;

	if (r > ARCHIVE_WARN)
		return (0);
	failure("%s: %s returned %d but left no errno", context, operation, r);
	if (!assert(archive_errno(a) != 0))
		failed = 1;
	failure("%s: %s returned %d but left no error string", context,
	    operation, r);
	if (!assert(archive_error_string(a) != NULL))
		failed = 1;
	return (failed);
}

/*
 * Read all of buff[0 .. size - 1], each entry's data either read or
 * skipped. Returns 1 when an operation failed without setting its error.
 * A failed header read starts from a clear error state, so that an error
 * left by an earlier operation cannot hide a missing one. Data operations
 * keep the state of their entry, because a reader may fail them for a
 * reason it reported while reading the header.
 */
static int
read_through(const char *buff, size_t size, int streaming, int skip,
    const char *context)
{
	struct stream stream;
	struct archive_entry *ae;
	struct archive *a;
	char tmp[4096];
	la_ssize_t n;
	int count, failed = 0, r;

	stream.data = buff;
	stream.size = size;
	stream.offset = 0;
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	if (streaming)
		r = archive_read_open2(a, &stream, NULL, read_stream, NULL,
		    NULL);
	else
		r = archive_read_open_memory(a, buff, size);
	if (check_error(a, context, "open", r))
		failed = 1;
	for (count = 0; r == ARCHIVE_OK && !failed && count < 100; count++) {
		archive_clear_error(a);
		r = archive_read_next_header(a, &ae);
		if (check_error(a, context, "next_header", r))
			failed = 1;
		if (r == ARCHIVE_EOF || r == ARCHIVE_FATAL)
			break;
		if (skip) {
			r = archive_read_data_skip(a);
			if (check_error(a, context, "data_skip", r))
				failed = 1;
		} else {
			while ((n = archive_read_data(a, tmp, sizeof(tmp))) > 0)
				;
			r = (n < 0) ? (int)n : ARCHIVE_OK;
			if (check_error(a, context, "read_data", r))
				failed = 1;
		}
		if (r == ARCHIVE_FATAL)
			break;
		r = ARCHIVE_OK;
	}
	if (!failed) {
		archive_clear_error(a);
		r = archive_read_close(a);
		if (check_error(a, context, "close", r))
			failed = 1;
	}
	archive_read_free(a);
	return (failed);
}

/* Cut buff at positions from 0 to size, and damage it at 0 to size - 1. */
static void
sweep(const char *name, const char *buff, size_t size)
{
	char context[200];
	char *damaged;
	size_t i, step = size / 300 + 1;
	int streaming, skip;

	assert((damaged = malloc(size)) != NULL);
	for (streaming = 0; streaming < 2; streaming++) {
		for (skip = 0; skip < 2; skip++) {
			for (i = 0; i <= size; i += step) {
				snprintf(context, sizeof(context),
				    "%s cut at %d of %d, %s, %s", name,
				    (int)i, (int)size,
				    streaming ? "streaming" : "seekable",
				    skip ? "skipping" : "reading");
				if (read_through(buff, i, streaming, skip,
				    context))
					goto done;
			}
			for (i = 0; i < size; i += step) {
				memcpy(damaged, buff, size);
				damaged[i] ^= 0xff;
				snprintf(context, sizeof(context),
				    "%s byte %d of %d damaged, %s, %s", name,
				    (int)i, (int)size,
				    streaming ? "streaming" : "seekable",
				    skip ? "skipping" : "reading");
				if (read_through(damaged, size, streaming, skip,
				    context))
					goto done;
			}
		}
	}
done:
	free(damaged);
}

DEFINE_TEST(test_read_error_state)
{
	size_t i, size;
	char *buff;

	for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
		if (sources[i].reference != NULL) {
			extract_reference_file(sources[i].reference);
			buff = slurpfile(&size, "%s", sources[i].reference);
		} else {
			buff = write_sample(sources[i].format, &size);
			if (buff == NULL) {
				skipping("%s writing is not supported on this "
				    "platform", sources[i].name);
				continue;
			}
		}
		assert(buff != NULL);
		if (buff == NULL)
			continue;
		sweep(sources[i].name, buff, size);
		free(buff);
	}
}
