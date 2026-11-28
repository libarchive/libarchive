/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Florian Bauckholt
 * All rights reserved.
 */
#include "test.h"

/*
 * Tar entries that carry no data (devices, FIFOs, directories, hard links
 * whose size is ignored) still have a size field.  archive_read_data() must
 * not return that many zero bytes for them: a 1.5 KiB archive whose FIFO
 * header claims 2^40 bytes used to make the read loop below return a
 * terabyte of zeros, also when the header only reads with a warning.  The
 * same holds for the GNU "realsize" field and sparse map of an entry that
 * is not an old-style sparse ('S') file.  Directories that do carry a body
 * (GNU dumpdir, 'D') must still return it.
 */

static void
set_checksum(char *h)
{
	unsigned sum = 0;
	int i;

	memset(h + 148, ' ', 8);
	for (i = 0; i < 512; i++)
		sum += (unsigned char)h[i];
	snprintf(h + 148, 8, "%06o", sum);
	h[155] = ' ';
}

/* Base-256 encoding of a 12-byte numeric field. */
static void
set_base256(char *field, int64_t v)
{
	int i;

	memset(field, 0, 12);
	field[0] = (char)0x80;
	for (i = 11; i > 0; i--, v >>= 8)
		field[i] = (char)(v & 0xff);
}

static void
make_header(char *buff, const char *name, char type, int gnu)
{
	memset(buff, 0, 1536);
	strcpy(buff, name);
	memcpy(buff + 100, "0000644", 8);
	buff[156] = type;
	if (gnu)
		memcpy(buff + 257, "ustar  ", 8);
	else
		memcpy(buff + 257, "ustar\0" "00", 8);
}

/*
 * Read the first entry's body; return how many bytes came back (capped),
 * or -1 if the options are not supported here.
 */
static int64_t
read_body_opt(const char *buff, size_t size, const char *options)
{
	struct archive_entry *ae;
	struct archive *a;
	char data[4096];
	int64_t total = 0;
	la_ssize_t r;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	if (options != NULL &&
	    ARCHIVE_OK != archive_read_set_options(a, options)) {
		skipping("This system cannot convert character-set"
		    " from UTF-8.");
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return (-1);
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, size));
	r = archive_read_next_header(a, &ae);
	if (options == NULL)
		assertEqualIntA(a, ARCHIVE_OK, r);
	else
		/* The name may or may not convert, depending on the locale. */
		assert(r == ARCHIVE_OK || r == ARCHIVE_WARN);
	while ((r = archive_read_data(a, data, sizeof(data))) > 0) {
		total += r;
		if (total > (1 << 20))
			break;
	}
	assertEqualInt(0, r);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	return (total);
}

static int64_t
read_body(const char *buff, size_t size)
{
	return (read_body_opt(buff, size, NULL));
}

DEFINE_TEST(test_read_format_tar_nodata_size)
{
	static const char types[] = { '3', '4', '5', '6' };
	char buff[1536];
	int64_t n;
	size_t i;

	for (i = 0; i < sizeof(types); i++) {
		make_header(buff, "node", types[i], 0);
		set_base256(buff + 124, (int64_t)1 << 40);
		set_checksum(buff);
		failure("typeflag %c", types[i]);
		assertEqualInt(0, read_body(buff, sizeof(buff)));
	}

	/* The same, with a name that is not valid UTF-8. */
	make_header(buff, "node\xff", '6', 0);
	set_base256(buff + 124, (int64_t)1 << 40);
	set_checksum(buff);
	n = read_body_opt(buff, sizeof(buff), "hdrcharset=UTF-8");
	if (n >= 0)
		assertEqualInt(0, n);

	/* A GNU dumpdir is a directory with a body. */
	make_header(buff, "dir/", 'D', 1);
	memcpy(buff + 124, "00000000005", 12);
	set_checksum(buff);
	memcpy(buff + 512, "Yabc", 5);
	assertEqualInt(5, read_body(buff, sizeof(buff)));

	/* A "regular file" whose name ends in '/' is a directory. */
	make_header(buff, "dir/", '0', 0);
	set_base256(buff + 124, (int64_t)1 << 40);
	set_checksum(buff);
	assertEqualInt(0, read_body(buff, sizeof(buff)));

	/* An old-style hard link: its size field is ignored. */
	make_header(buff, "link", '1', 0);
	memset(buff + 257, 0, 8);
	strcpy(buff + 157, "node");
	set_base256(buff + 124, (int64_t)1 << 40);
	set_checksum(buff);
	assertEqualInt(0, read_body(buff, sizeof(buff)));

	/* GNU realsize on a regular (non-'S') entry with one byte of data. */
	make_header(buff, "file", '0', 1);
	memcpy(buff + 124, "00000000001", 12);
	set_base256(buff + 483, (int64_t)1 << 40);
	set_checksum(buff);
	buff[512] = 'x';
	assertEqualInt(1, read_body(buff, sizeof(buff)));

	/* An old GNU sparse map on a regular (non-'S') entry. */
	make_header(buff, "file", '0', 1);
	memcpy(buff + 124, "00000000001", 12);
	set_base256(buff + 386, (int64_t)1 << 40);
	memcpy(buff + 398, "00000000001", 12);
	set_checksum(buff);
	buff[512] = 'x';
	assertEqualInt(1, read_body(buff, sizeof(buff)));
}
