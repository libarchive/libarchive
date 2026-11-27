/*-
 * Copyright (c) 2026 Tim Kientzle
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
 * POSIX gives the linkname field meaning only for hardlinks and
 * symlinks; the typeflag alone decides what kind of entry a member is.
 * These tests check that a link target attached to a regular file --
 * whether in the ustar linkname field, a pax `linkpath` record, or a
 * GNU longlink ('K') entry -- does not turn the file into a link.
 *
 * The archives are modeled on cases from the tar-minefield suite
 * (https://github.com/haampie/tar-minefield):
 *   K02-linkname-on-file: ustar regular file with linkname "target"
 *   K16-pax-linkpath-on-file: pax `linkpath=target` before a regular file
 * The GNU longlink variant has the same shape as K16.
 * See https://github.com/libarchive/libarchive/issues/3901
 *
 * The archives are built in memory here rather than stored as
 * reference files.
 */

static const char file_data[] = "a file, or a link?\n";

/* Fill in one 512-byte tar header block. */
static void
make_header(char *h, const char *name, char typeflag, const char *linkname,
    size_t size)
{
	unsigned int sum;
	int i;

	memset(h, 0, 512);
	strcpy(h + 0, name);			/* name */
	strcpy(h + 100, "0000644");		/* mode */
	strcpy(h + 108, "0000000");		/* uid */
	strcpy(h + 116, "0000000");		/* gid */
	snprintf(h + 124, 12, "%011o", (unsigned int)size); /* size */
	strcpy(h + 136, "00000000000");		/* mtime */
	h[156] = typeflag;			/* typeflag */
	if (linkname != NULL)
		strcpy(h + 157, linkname);	/* linkname */
	memcpy(h + 257, "ustar", 6);		/* magic */
	memcpy(h + 263, "00", 2);		/* version */
	strcpy(h + 329, "0000000");		/* devmajor */
	strcpy(h + 337, "0000000");		/* devminor */

	/* Checksum is computed with the checksum field set to spaces. */
	memset(h + 148, ' ', 8);
	for (sum = 0, i = 0; i < 512; i++)
		sum += (unsigned char)h[i];
	snprintf(h + 148, 8, "%06o", sum);	/* "NNNNNN\0 " */
}

/*
 * Build an archive: an optional extension header (typeflag `ext_type`
 * with body `ext_body`), then a member `a` with typeflag `typeflag`,
 * ustar linkname `linkname`, and body `file_data` if it is a regular
 * file.  Returns the archive size.
 */
static size_t
make_archive(char *buff, char ext_type, const char *ext_body,
    char typeflag, const char *linkname)
{
	char *p = buff;
	size_t data_size = (typeflag == '0') ? strlen(file_data) : 0;

	memset(buff, 0, 512 * 6);
	if (ext_body != NULL) {
		make_header(p,
		    ext_type == 'K' ? "././@LongLink" : "PaxHeader",
		    ext_type, NULL, strlen(ext_body));
		p += 512;
		memcpy(p, ext_body, strlen(ext_body));
		p += 512;
	}
	make_header(p, "a", typeflag, linkname, data_size);
	p += 512;
	if (data_size > 0) {
		memcpy(p, file_data, data_size);
		p += 512;
	}
	p += 1024;	/* Two zero blocks mark end-of-archive. */
	return (p - buff);
}

/* Read the archive and verify it holds one regular file `a`. */
static void
verify_regular_file(const char *buff, size_t size)
{
	struct archive *a;
	struct archive_entry *ae;
	char data[64];

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, size));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("a", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualString(NULL, archive_entry_hardlink(ae));
	assertEqualString(NULL, archive_entry_symlink(ae));
	assertEqualInt(strlen(file_data), archive_entry_size(ae));
	memset(data, 0, sizeof(data));
	assertEqualInt(strlen(file_data),
	    archive_read_data(a, data, sizeof(data)));
	assertEqualString(file_data, data);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* Read the archive and verify it holds one hardlink or symlink `a` -> `target`. */
static void
verify_link(const char *buff, size_t size, int is_symlink)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, size));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("a", archive_entry_pathname(ae));
	if (is_symlink) {
		assertEqualInt(AE_IFLNK, archive_entry_filetype(ae));
		assertEqualString("target", archive_entry_symlink(ae));
		assertEqualString(NULL, archive_entry_hardlink(ae));
	} else {
		assertEqualString("target", archive_entry_hardlink(ae));
		assertEqualString(NULL, archive_entry_symlink(ae));
	}

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_tar_linkname_on_file)
{
	char buff[512 * 6];
	size_t size;

	/* K02: ustar regular file with a linkname. */
	size = make_archive(buff, 0, NULL, '0', "target");
	verify_regular_file(buff, size);

	/* K16: pax `linkpath` before a regular file. */
	size = make_archive(buff, 'x', "19 linkpath=target\n", '0', NULL);
	verify_regular_file(buff, size);

	/* Same, but with a GNU longlink entry instead of pax. */
	size = make_archive(buff, 'K', "target", '0', NULL);
	verify_regular_file(buff, size);

	/* Pax `linkpath` still applies to hardlinks and symlinks. */
	size = make_archive(buff, 'x', "19 linkpath=target\n", '1', NULL);
	verify_link(buff, size, 0);
	size = make_archive(buff, 'x', "19 linkpath=target\n", '2', NULL);
	verify_link(buff, size, 1);

	/* As do GNU longlink entries. */
	size = make_archive(buff, 'K', "target", '1', NULL);
	verify_link(buff, size, 0);
	size = make_archive(buff, 'K', "target", '2', NULL);
	verify_link(buff, size, 1);
}
