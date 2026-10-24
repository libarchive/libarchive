/*-
 * Copyright (c) 2026 François Degros
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

#define	BUFFER_SIZE	65536

static void
check_stem(const char *filename, int expected)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	failure("%s", filename);
	assertEqualInt(expected,
	    (int)archive_read_set_format_from_filename(a, filename, -1));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_set_format_from_filename_stem)
{
	struct archive *a;

	/* Archives, with the length of the name without its extension. */
	check_stem("a.tar", 1);
	check_stem("report.zip", 6);
	check_stem("report.7z", 6);
	check_stem("dir.d/sub/report.docx", 16);
	check_stem("REPORT.TAR", 6);

	/* Compressed archives and compressed files. */
	check_stem("report.tar.gz", 6);
	check_stem("report.tgz", 6);
	check_stem("report.txt.gz", 10);
	check_stem("report.txt.gz.xz", 10);
	check_stem("report.tar.xz.gz", 6);
	check_stem("report.7z.gz", 9);
	check_stem("report.zip.bz2", 10);
	check_stem("report.cbr.gz", 6);
	check_stem("report.cbt.xz", 6);

	/* Uuencoded files, with the archives that can follow the filters. */
	check_stem("report.uue", 6);
	check_stem("report.txt.gz.uu", 10);
	check_stem("report.tar.uu", 6);
	check_stem("report.tgz.uu", 6);
	check_stem("report.zip.uu", 10);

	/* A compressed cpio archive, and more archive extensions. */
	check_stem("report.cpgz", 6);
	check_stem("report.cpgz.uu", 6);
	check_stem("report.ddeb", 6);
	check_stem("report.msu", 6);
	check_stem("report.xps", 6);
	check_stem("report.vsdx", 6);
	check_stem("report.usdz", 6);
	check_stem("level.pk3", 5);
	check_stem("report.sxw", 6);
	check_stem("report.xlam", 6);

	/* The longest extensions in the tables are 10 characters. */
	check_stem("app.appxbundle", 3);
	check_stem("app.msixbundle", 3);
	check_stem("pack.mctemplate", 4);

	/* Extensions that mean different things are left alone. */
	check_stem("report.key", ARCHIVE_FAILED);
	check_stem("report.pkg", ARCHIVE_FAILED);
	check_stem("report.lib", ARCHIVE_FAILED);
	check_stem("report.ipk", ARCHIVE_FAILED);
	check_stem("report.img", ARCHIVE_FAILED);

	/* RPM packages. */
	check_stem("report.rpm", 6);
	check_stem("report.src.rpm", 10);
	check_stem("report.spm", 6);
	/* The payload filters of an RPM must bid, so it isn't read after one. */
	check_stem("report.rpm.gz", 10);

	/* Not recognized. */
	check_stem("", ARCHIVE_FAILED);
	check_stem("report", ARCHIVE_FAILED);
	check_stem("report.", ARCHIVE_FAILED);
	check_stem("report.unknown", ARCHIVE_FAILED);
	check_stem("report.toolongextension", ARCHIVE_FAILED);
	check_stem("report.abcdefghij", ARCHIVE_FAILED);
	check_stem("report.abcdefghijk", ARCHIVE_FAILED);
	check_stem(".gz", ARCHIVE_FAILED);
	check_stem("dir/.gz", ARCHIVE_FAILED);
	check_stem("..gz", ARCHIVE_FAILED);
	check_stem("dir.tar/report", ARCHIVE_FAILED);
	check_stem("report tar", ARCHIVE_FAILED);
	check_stem("report.tar gz", ARCHIVE_FAILED);

	/* An explicit length. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(10,
	    (int)archive_read_set_format_from_filename(a, "report.txt.gzip", 13));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(ARCHIVE_FAILED,
	    (int)archive_read_set_format_from_filename(a, "report.txt.gzip", 14));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* A length of zero is an empty name, not "use strlen()". */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(ARCHIVE_FAILED,
	    (int)archive_read_set_format_from_filename(a, "report.gz", 0));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* The name needn't be terminated when its length is given. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(6,
	    (int)archive_read_set_format_from_filename(a, "report.tgzXYZ", 10));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* A name with a NUL byte has no reliable extension. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(ARCHIVE_FAILED,
	    (int)archive_read_set_format_from_filename(a, "report.gz\0x", 11));
	assertEqualInt(ARCHIVE_FAILED,
	    (int)archive_read_set_format_from_filename(a, "report\0.gz", 10));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* A missing name stands for standard input: there is no extension. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(ARCHIVE_FAILED,
	    (int)archive_read_set_format_from_filename(a, NULL, -1));
	assertEqualInt(ARCHIVE_FAILED,
	    (int)archive_read_set_format_from_filename(a, NULL, 0));
	assertEqualString("Unrecognized filename extension",
	    archive_error_string(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_set_format_from_filename_unrecognized)
{
	static const char data[] = "not an archive";
	struct archive *a;

	/* Nothing is registered when the name is not recognized. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(ARCHIVE_FAILED,
	    (int)archive_read_set_format_from_filename(a, "report.unknown", -1));
	assertEqualString("Unrecognized filename extension",
	    archive_error_string(a));
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_open_memory(a, data, sizeof(data) - 1));
	assertEqualString("No formats registered", archive_error_string(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Writes a file named "name" with the given mode and content in the given
 * format, and through the given filter if there is one. Returns the size.
 */
static size_t
make_archive(char *buff, int (*set_format)(struct archive *),
    int (*add_filter)(struct archive *), const char *name, int mode,
    const void *data, size_t size)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t used = 0;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, set_format(a));
	if (add_filter != NULL)
		assertEqualIntA(a, ARCHIVE_OK, add_filter(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, BUFFER_SIZE, &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, name);
	archive_entry_set_mtime(ae, 1, 0);
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, mode);
	archive_entry_set_size(ae, (la_int64_t)size);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, (int)size, archive_write_data(a, data, size));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	return (used);
}

static int
raw_format(struct archive *a)
{
	return (archive_write_set_format_raw(a));
}

static int
zip_format(struct archive *a)
{
	return (archive_write_set_format_zip(a));
}

static int
sevenzip_format(struct archive *a)
{
	return (archive_write_set_format_7zip(a));
}

static int
tar_format(struct archive *a)
{
	return (archive_write_set_format_pax_restricted(a));
}

static int
cpio_format(struct archive *a)
{
	return (archive_write_set_format_cpio_newc(a));
}

static int
gzip_filter(struct archive *a)
{
	return (archive_write_add_filter_gzip(a));
}

DEFINE_TEST(test_read_set_format_from_filename_read)
{
	static const char text[] = "hello, world\n";
	char *tgz, *gz, *zip, *zip_gz, *sevenzip, *sevenzip_gz;
	size_t tgz_size, gz_size, zip_size, zip_gz_size, sevenzip_size,
	    sevenzip_gz_size;
	char buff[64];
	struct archive *a;
	struct archive_entry *ae;

	if (archive_zlib_version() == NULL) {
		skipping("gzip support is not available");
		return;
	}

	assert((tgz = malloc(BUFFER_SIZE)) != NULL);
	assert((gz = malloc(BUFFER_SIZE)) != NULL);
	assert((zip = malloc(BUFFER_SIZE)) != NULL);
	assert((zip_gz = malloc(BUFFER_SIZE)) != NULL);
	assert((sevenzip = malloc(BUFFER_SIZE)) != NULL);
	assert((sevenzip_gz = malloc(BUFFER_SIZE)) != NULL);

	tgz_size = make_archive(tgz, tar_format, gzip_filter, "file.txt",
	    0644, text, sizeof(text) - 1);
	gz_size = make_archive(gz, raw_format, gzip_filter, "ignored", 0644,
	    text, sizeof(text) - 1);
	zip_size = make_archive(zip, zip_format, NULL, "run.sh", 0755,
	    text, sizeof(text) - 1);
	zip_gz_size = make_archive(zip_gz, raw_format, gzip_filter, "ignored",
	    0644, zip, zip_size);
	sevenzip_size = make_archive(sevenzip, sevenzip_format, NULL, "a.txt",
	    0644, text, sizeof(text) - 1);
	sevenzip_gz_size = make_archive(sevenzip_gz, raw_format, gzip_filter,
	    "ignored", 0644, sevenzip, sevenzip_size);

	/* A compressed tar archive. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(6,
	    (int)archive_read_set_format_from_filename(a, "report.tgz", -1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, tgz, tgz_size));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file.txt", archive_entry_pathname(ae));
	assertEqualInt(sizeof(text) - 1,
	    archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, text, sizeof(text) - 1);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* A compressed file. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(10, (int)archive_read_set_format_from_filename(a,
	    "report.txt.gz", -1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, gz, gz_size));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("report.txt", archive_entry_pathname(ae));
	assertEqualInt(sizeof(text) - 1,
	    archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, text, sizeof(text) - 1);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Only the last component of the name is used for the entry. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(21, (int)archive_read_set_format_from_filename(a,
	    "dir/../sub/report.txt.gz", -1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, gz, gz_size));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("report.txt", archive_entry_pathname(ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* A ZIP archive keeps the Unix mode of its entries. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(6,
	    (int)archive_read_set_format_from_filename(a, "report.zip", -1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, zip, zip_size));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("run.sh", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * A ZIP archive that was compressed afterwards is a compressed file:
	 * reading it as a ZIP archive would need to seek.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(10,
	    (int)archive_read_set_format_from_filename(a, "report.zip.gz", -1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, zip_gz, zip_gz_size));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_FORMAT_RAW, archive_format(a));
	assertEqualString("report.zip", archive_entry_pathname(ae));
	assertEqualInt((int)zip_size, (int)archive_read_data(a, zip, zip_size));
	assertEqualMem(zip, "PK", 2);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Same for a 7-Zip archive, which can't even be read as a stream. */
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(9,
	    (int)archive_read_set_format_from_filename(a, "report.7z.gz", -1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, sevenzip_gz, sevenzip_gz_size));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_FORMAT_RAW, archive_format(a));
	assertEqualString("report.7z", archive_entry_pathname(ae));
	assertEqualInt((int)sevenzip_size,
	    (int)archive_read_data(a, sevenzip, sevenzip_size));
	assertEqualMem(sevenzip, "7z\xBC\xAF\x27\x1C", 6);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* It must be called before the archive is opened. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_gzip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, tgz, tgz_size));
	assertEqualInt(ARCHIVE_FATAL,
	    (int)archive_read_set_format_from_filename(a, "report.tgz", -1));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(tgz);
	free(gz);
	free(zip);
	free(zip_gz);
	free(sevenzip);
	free(sevenzip_gz);
}

static void
test_rpm(const char *name, int payload_filter, const char *payload_name)
{
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_file(name);
	assert((a = archive_read_new()) != NULL);
	assertEqualInt((int)strlen(name) - 4,
	    (int)archive_read_set_format_from_filename(a, name, -1));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, name, 2));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("./etc/file1", archive_entry_pathname(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("./etc/file2", archive_entry_pathname(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("./etc/file3", archive_entry_pathname(ae));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* The payload filter was found by its contents, below the RPM one. */
	assertEqualInt(payload_filter, archive_filter_code(a, 0));
	assertEqualString(payload_name, archive_filter_name(a, 0));
	assertEqualInt(ARCHIVE_FILTER_RPM, archive_filter_code(a, 1));
	assertEqualInt(ARCHIVE_FORMAT_CPIO_SVR4_NOCRC, archive_format(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_set_format_from_filename_rpm)
{
	if (archive_zlib_version() != NULL) {
		test_rpm("test_read_format_cpio_svr4_gzip_rpm.rpm",
		    ARCHIVE_FILTER_GZIP, "gzip");
	} else {
		skipping("gzip support is not available");
	}

	if (archive_bzlib_version() != NULL) {
		test_rpm("test_read_format_cpio_svr4_bzip2_rpm.rpm",
		    ARCHIVE_FILTER_BZIP2, "bzip2");
	} else {
		skipping("bzip2 support is not available");
	}
}

DEFINE_TEST(test_read_set_format_from_filename_cpgz)
{
	static const char text[] = "hello, world\n";
	char *cpgz;
	size_t size;
	char buff[64];
	struct archive *a;
	struct archive_entry *ae;

	if (archive_zlib_version() == NULL) {
		skipping("gzip support is not available");
		return;
	}

	assert((cpgz = malloc(BUFFER_SIZE)) != NULL);
	size = make_archive(cpgz, cpio_format, gzip_filter, "file.txt", 0644,
	    text, sizeof(text) - 1);

	assert((a = archive_read_new()) != NULL);
	assertEqualInt(6,
	    (int)archive_read_set_format_from_filename(a, "report.cpgz", -1));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, cpgz, size));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file.txt", archive_entry_pathname(ae));
	assertEqualInt(sizeof(text) - 1,
	    archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, text, sizeof(text) - 1);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_FILTER_GZIP, archive_filter_code(a, 0));
	assertEqualInt(ARCHIVE_FORMAT_CPIO_SVR4_NOCRC, archive_format(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(cpgz);
}

/* An empty compressed file is an empty archive, as it is for bsdcat. */
DEFINE_TEST(test_read_set_format_from_filename_empty)
{
	char *gz;
	size_t size;
	struct archive *a;
	struct archive_entry *ae;

	if (archive_zlib_version() == NULL) {
		skipping("gzip support is not available");
		return;
	}

	assert((gz = malloc(BUFFER_SIZE)) != NULL);
	size = make_archive(gz, raw_format, gzip_filter, "ignored", 0644, "",
	    0);

	assert((a = archive_read_new()) != NULL);
	assertEqualInt(5,
	    (int)archive_read_set_format_from_filename(a, "empty.gz", -1));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, gz, size));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(gz);
}
