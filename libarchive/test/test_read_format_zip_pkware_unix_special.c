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

/*
 * PKWARE's Unix extra field (0x000D) ends with data that depends on the file
 * type: the major and minor numbers of a device, or the name of the file that
 * a link points to. PKZIP for Unix stores the target of a symlink there, and
 * not in the data of the entry. It also marks a hard link with a flag in the
 * external attributes, and says that MS-DOS made the entry although the
 * external attributes hold a Unix mode.
 *
 * Each test builds a ZIP file in memory and reads it back.
 */

#define MAX_ENTRIES	8
#define MAX_ZIP_SIZE	4096

static void
put16(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v & 0xff);
	p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void
put32(unsigned char *p, unsigned long v)
{
	put16(p, (unsigned)(v & 0xffff));
	put16(p + 2, (unsigned)((v >> 16) & 0xffff));
}

struct zip_builder {
	unsigned char zip[MAX_ZIP_SIZE];
	size_t size;
	/* The Central Directory records, written at the end. */
	unsigned char cd[MAX_ZIP_SIZE];
	size_t cd_size;
	int count;
};

/*
 * Adds a stored entry with a PKWARE Unix extra field, made of the fixed
 * metadata and of the given variable data, to both its Local Header and its
 * Central Directory record.
 */
static void
add_entry(struct zip_builder *b, const char *name, unsigned mode,
    unsigned made_by, unsigned attributes_flags, const void *var,
    size_t var_size, const char *data)
{
	unsigned char extra[128];
	const size_t name_size = strlen(name);
	const size_t data_size = strlen(data);
	size_t extra_size, n;

	assert(var_size <= 64);

	put16(extra, 0x000d);
	put16(extra + 2, (unsigned)(12 + var_size));
	put32(extra + 4, 1700000000UL);	/* access time */
	put32(extra + 8, 1700000100UL);	/* modification time */
	put16(extra + 12, 1000);	/* UID */
	put16(extra + 14, 1001);	/* GID */
	if (var_size > 0)
		memcpy(extra + 16, var, var_size);
	extra_size = 16 + var_size;

	/* Local Header. */
	n = b->size;
	put32(b->zip + n, 0x04034b50UL);
	put16(b->zip + n + 4, 10);		/* version needed */
	put16(b->zip + n + 6, 0);		/* flags */
	put16(b->zip + n + 8, 0);		/* stored */
	put16(b->zip + n + 10, 0);		/* time */
	put16(b->zip + n + 12, 0x21);		/* date */
	put32(b->zip + n + 14, 0);		/* CRC-32 (not checked) */
	put32(b->zip + n + 18, (unsigned long)data_size);
	put32(b->zip + n + 22, (unsigned long)data_size);
	put16(b->zip + n + 26, (unsigned)name_size);
	put16(b->zip + n + 28, (unsigned)extra_size);
	n += 30;
	memcpy(b->zip + n, name, name_size);
	n += name_size;
	memcpy(b->zip + n, extra, extra_size);
	n += extra_size;
	memcpy(b->zip + n, data, data_size);
	n += data_size;

	/* Central Directory record. */
	{
		unsigned char *c = b->cd + b->cd_size;

		put32(c, 0x02014b50UL);
		put16(c + 4, (made_by << 8) | 20);	/* version made by */
		put16(c + 6, 10);			/* version needed */
		put16(c + 8, 0);			/* flags */
		put16(c + 10, 0);			/* stored */
		put16(c + 12, 0);			/* time */
		put16(c + 14, 0x21);			/* date */
		put32(c + 16, 0);			/* CRC-32 */
		put32(c + 20, (unsigned long)data_size);
		put32(c + 24, (unsigned long)data_size);
		put16(c + 28, (unsigned)name_size);
		put16(c + 30, (unsigned)extra_size);
		put16(c + 32, 0);			/* comment size */
		put16(c + 34, 0);			/* disk number */
		put16(c + 36, 0);			/* internal attributes */
		put32(c + 38, ((unsigned long)mode << 16) | attributes_flags);
		put32(c + 42, (unsigned long)b->size);	/* offset */
		memcpy(c + 46, name, name_size);
		memcpy(c + 46 + name_size, extra, extra_size);
		b->cd_size += 46 + name_size + extra_size;
	}

	b->size = n;
	b->count++;
	assert(b->count <= MAX_ENTRIES);
	assert(b->size + b->cd_size + 22 <= MAX_ZIP_SIZE);
}

/* Appends the Central Directory, and returns the size of the ZIP file. */
static size_t
finish(struct zip_builder *b)
{
	size_t n = b->size;

	memcpy(b->zip + n, b->cd, b->cd_size);
	n += b->cd_size;
	put32(b->zip + n, 0x06054b50UL);
	put16(b->zip + n + 4, 0);
	put16(b->zip + n + 6, 0);
	put16(b->zip + n + 8, (unsigned)b->count);
	put16(b->zip + n + 10, (unsigned)b->count);
	put32(b->zip + n + 12, (unsigned long)b->cd_size);
	put32(b->zip + n + 16, (unsigned long)b->size);
	put16(b->zip + n + 20, 0);
	return (n + 22);
}

static struct archive *
open_zip(struct zip_builder *b, int seekable)
{
	struct archive *a;
	size_t size = finish(b);

	assert((a = archive_read_new()) != NULL);
	if (seekable)
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_zip_seekable(a));
	else
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_zip_streamable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, b->zip, size));
	return (a);
}

/* The target of a symlink is in the extra field when the data is empty. */
DEFINE_TEST(test_read_format_zip_pkware_unix_symlink)
{
	static struct zip_builder b;
	struct archive_entry *ae;
	struct archive *a;

	memset(&b, 0, sizeof(b));
	/* Made by Unix. */
	add_entry(&b, "symlink", AE_IFLNK | 0777, 3, 0, "target.txt", 10, "");
	/* Made by MS-DOS, as PKZIP for Unix does. */
	add_entry(&b, "dos-symlink", AE_IFLNK | 0777, 0, 0, "target.txt", 10,
	    "");
	/* The data of the entry is preferred. */
	add_entry(&b, "both", AE_IFLNK | 0777, 3, 0, "from-extra", 10,
	    "from-data");
	/* Hard link flag: it is a symlink to the name in the field. PKZIP
	 * sets it on symlinks with more than one link. */
	add_entry(&b, "flagged", AE_IFLNK | 0777, 0, 0x800, "target.txt", 10,
	    "");
	/* Nothing in the extra field: the target stays empty. */
	add_entry(&b, "empty", AE_IFLNK | 0777, 3, 0, "", 0, "");

	a = open_zip(&b, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("symlink", archive_entry_pathname(ae));
	assertEqualInt(AE_IFLNK, archive_entry_filetype(ae));
	assertEqualString("target.txt", archive_entry_symlink(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dos-symlink", archive_entry_pathname(ae));
	assertEqualInt(AE_IFLNK, archive_entry_filetype(ae));
	assertEqualInt(0777, archive_entry_perm(ae));
	assertEqualString("target.txt", archive_entry_symlink(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("both", archive_entry_pathname(ae));
	assertEqualString("from-data", archive_entry_symlink(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("flagged", archive_entry_pathname(ae));
	assertEqualInt(AE_IFLNK, archive_entry_filetype(ae));
	assertEqualString("target.txt", archive_entry_symlink(ae));
	assertEqualString(NULL, archive_entry_hardlink(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("empty", archive_entry_pathname(ae));
	assertEqualString("", archive_entry_symlink(ae));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* A hard link has a flag in the external attributes, and the name of the
 * original file in the extra field. */
DEFINE_TEST(test_read_format_zip_pkware_unix_hardlink)
{
	static struct zip_builder b;
	struct archive_entry *ae;
	struct archive *a;

	memset(&b, 0, sizeof(b));
	add_entry(&b, "original.txt", AE_IFREG | 0644, 3, 0, "", 0, "hello\n");
	add_entry(&b, "link1", AE_IFREG | 0644, 3, 0x800, "original.txt", 12,
	    "");
	/* A link to a link. */
	add_entry(&b, "link2", AE_IFREG | 0644, 0, 0x800, "link1", 5, "");
	/* Without the flag, the name is not a hard link. */
	add_entry(&b, "not-a-link", AE_IFREG | 0644, 3, 0, "original.txt", 12,
	    "");
	/* A directory is never a hard link. */
	add_entry(&b, "dir/", AE_IFDIR | 0755, 3, 0x800, "original.txt", 12,
	    "");
	/* The flag without a name is ignored. */
	add_entry(&b, "no-name", AE_IFREG | 0644, 3, 0x800, "", 0, "");

	a = open_zip(&b, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("original.txt", archive_entry_pathname(ae));
	assertEqualString(NULL, archive_entry_hardlink(ae));
	assertEqualInt(6, archive_entry_size(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("link1", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualString("original.txt", archive_entry_hardlink(ae));
	assertEqualInt(0, archive_entry_size(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("link2", archive_entry_pathname(ae));
	assertEqualString("link1", archive_entry_hardlink(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("not-a-link", archive_entry_pathname(ae));
	assertEqualString(NULL, archive_entry_hardlink(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dir/", archive_entry_pathname(ae));
	assertEqualInt(AE_IFDIR, archive_entry_filetype(ae));
	assertEqualString(NULL, archive_entry_hardlink(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("no-name", archive_entry_pathname(ae));
	assertEqualString(NULL, archive_entry_hardlink(ae));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* The major and minor numbers of a device are in the extra field. */
DEFINE_TEST(test_read_format_zip_pkware_unix_device)
{
	static struct zip_builder b;
	struct archive_entry *ae;
	struct archive *a;
	unsigned char numbers[8];

	memset(&b, 0, sizeof(b));
	put32(numbers, 4);
	put32(numbers + 4, 64);
	add_entry(&b, "char", AE_IFCHR | 0620, 0, 0, numbers, 8, "");
	put32(numbers, 8);
	put32(numbers + 4, 1);
	add_entry(&b, "block", AE_IFBLK | 0660, 3, 0, numbers, 8, "");
	/* Too short: the device numbers are left unset. */
	add_entry(&b, "short", AE_IFCHR | 0620, 3, 0, numbers, 4, "");
	add_entry(&b, "none", AE_IFCHR | 0620, 3, 0, "", 0, "");

	a = open_zip(&b, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("char", archive_entry_pathname(ae));
	assertEqualInt(AE_IFCHR, archive_entry_filetype(ae));
	assertEqualInt(0620, archive_entry_perm(ae));
	assertEqualInt(4, archive_entry_rdevmajor(ae));
	assertEqualInt(64, archive_entry_rdevminor(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("block", archive_entry_pathname(ae));
	assertEqualInt(AE_IFBLK, archive_entry_filetype(ae));
	assertEqualInt(8, archive_entry_rdevmajor(ae));
	assertEqualInt(1, archive_entry_rdevminor(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("short", archive_entry_pathname(ae));
	assertEqualInt(AE_IFCHR, archive_entry_filetype(ae));
	assertEqualInt(0, archive_entry_rdevmajor(ae));
	assertEqualInt(0, archive_entry_rdevminor(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("none", archive_entry_pathname(ae));
	assertEqualInt(0, archive_entry_rdevmajor(ae));
	assertEqualInt(0, archive_entry_rdevminor(ae));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * The external attributes hold a Unix mode when PKZIP says that MS-DOS made
 * the entry, as long as it has a valid file type. Otherwise they are DOS
 * attributes.
 */
DEFINE_TEST(test_read_format_zip_pkware_unix_dos_attributes)
{
	static struct zip_builder b;
	struct archive_entry *ae;
	struct archive *a;

	memset(&b, 0, sizeof(b));
	/* A Unix mode, with a file type. */
	add_entry(&b, "unix", AE_IFREG | 0600, 0, 0, "", 0, "");
	add_entry(&b, "socket", AE_IFSOCK | 0600, 0, 0, "", 0, "");
	/* MS-DOS attributes: a read-only file, and a directory. */
	add_entry(&b, "dos-readonly", 0, 0, 0x01, "", 0, "");
	add_entry(&b, "dos-file", 0, 0, 0x20, "", 0, "");
	add_entry(&b, "dos-dir/", 0, 0, 0x10, "", 0, "");
	/* Not a file type: these are not a Unix mode. */
	add_entry(&b, "not-a-type", 0100, 0, 0x01, "", 0, "");

	a = open_zip(&b, 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("unix", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(0600, archive_entry_perm(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("socket", archive_entry_pathname(ae));
	assertEqualInt(AE_IFSOCK, archive_entry_filetype(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dos-readonly", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(0444, archive_entry_perm(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dos-file", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(0664, archive_entry_perm(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dos-dir/", archive_entry_pathname(ae));
	assertEqualInt(AE_IFDIR, archive_entry_filetype(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("not-a-type", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(0444, archive_entry_perm(ae));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * The streaming reader only sees the Local Headers, which have no external
 * attributes: it can't tell what the variable data of the extra field is for.
 * It reads the entries as before.
 */
DEFINE_TEST(test_read_format_zip_pkware_unix_streaming)
{
	static struct zip_builder b;
	struct archive_entry *ae;
	struct archive *a;
	unsigned char numbers[8];

	memset(&b, 0, sizeof(b));
	put32(numbers, 4);
	put32(numbers + 4, 64);
	add_entry(&b, "symlink", AE_IFLNK | 0777, 3, 0, "target.txt", 10, "");
	add_entry(&b, "link", AE_IFREG | 0644, 3, 0x800, "symlink", 7, "");
	add_entry(&b, "char", AE_IFCHR | 0620, 3, 0, numbers, 8, "");

	a = open_zip(&b, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("symlink", archive_entry_pathname(ae));
	assertEqualString(NULL, archive_entry_hardlink(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("link", archive_entry_pathname(ae));
	assertEqualString(NULL, archive_entry_hardlink(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("char", archive_entry_pathname(ae));
	assertEqualInt(0, archive_entry_rdevmajor(ae));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
