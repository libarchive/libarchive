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

#define __LIBARCHIVE_BUILD
#include <archive_endian.h>

/*
 * ECMA-119 6.1.2 only permits logical block sizes of 512, 1024, or
 * 2048 bytes.  A non-power-of-two logical block size (e.g., 65535 or
 * 2049) combined with a very large directory allowed the rounded
 * directory length in read_children() to be truncated on 32-bit
 * systems, which then underflowed the scan loop and read out of
 * bounds.  Such volume descriptors must be rejected.
 *
 * This test builds a minimal ISO image in memory with a given
 * logical block size and checks whether the ISO9660 reader accepts it.
 */

#define SECTOR 2048
#define ROOT_SECTOR 20
/* The bidder reads 16 + 8 sectors; leave room beyond that. */
#define IMAGE_SECTORS 32

static void
set_both16(unsigned char *p, unsigned v)
{
	archive_le16enc(p, (uint16_t)v);
	archive_be16enc(p + 2, (uint16_t)v);
}

static void
set_both32(unsigned char *p, uint32_t v)
{
	archive_le32enc(p, v);
	archive_be32enc(p + 4, v);
}

static void
set_dir_record(unsigned char *p, uint32_t location, uint32_t size,
    unsigned char name)
{
	p[0] = 34;			/* Record length */
	set_both32(p + 2, location);	/* Extent location */
	set_both32(p + 10, size);	/* Data length */
	p[18] = 126;			/* Recording date: 2026-01-01 */
	p[19] = 1;
	p[20] = 1;
	p[25] = 0x02;			/* Directory */
	set_both16(p + 28, 1);		/* Volume sequence number */
	p[32] = 1;			/* Name length */
	p[33] = name;
}

static void
build_image(unsigned char *buff, unsigned logical_block_size)
{
	unsigned char *pvd = buff + 16 * SECTOR;
	unsigned char *term = buff + 17 * SECTOR;
	unsigned char *root = buff + ROOT_SECTOR * SECTOR;
	uint32_t blocks_per_sector, root_block, volume_blocks;

	memset(buff, 0, IMAGE_SECTORS * SECTOR);

	/*
	 * Locations and the volume size are in logical blocks.  For
	 * nonsensical block sizes, just keep the 2048-byte layout so that
	 * every field other than the logical block size is plausible.
	 */
	blocks_per_sector = 1;
	if (logical_block_size > 0 && logical_block_size < SECTOR)
		blocks_per_sector = SECTOR / logical_block_size;
	root_block = ROOT_SECTOR * blocks_per_sector;
	volume_blocks = IMAGE_SECTORS * blocks_per_sector;

	/* Primary Volume Descriptor */
	pvd[0] = 1;
	memcpy(pvd + 1, "CD001", 5);
	pvd[6] = 1;
	memset(pvd + 8, ' ', 32);		/* System identifier */
	memset(pvd + 40, ' ', 32);		/* Volume identifier */
	set_both32(pvd + 80, volume_blocks);	/* Volume space size */
	set_both16(pvd + 120, 1);		/* Volume set size */
	set_both16(pvd + 124, 1);		/* Volume sequence number */
	set_both16(pvd + 128, logical_block_size);
	set_both32(pvd + 132, 10);		/* Path table size */
	archive_le32enc(pvd + 140, 18);		/* Type L path table */
	archive_be32enc(pvd + 148, 19);		/* Type M path table */
	set_dir_record(pvd + 156, root_block, logical_block_size, 0);
	pvd[881] = 1;				/* File structure version */

	/* Volume Descriptor Set Terminator */
	term[0] = 255;
	memcpy(term + 1, "CD001", 5);
	term[6] = 1;

	/* Root directory: just "." and ".." */
	set_dir_record(root, root_block, logical_block_size, 0);
	set_dir_record(root + 34, root_block, logical_block_size, 1);
}

/*
 * Open the image and, if requested, read through to the end.
 * When only opening, the result reflects whether the ISO9660
 * bidder recognized the image at all.
 */
static int
read_image(unsigned logical_block_size, int read_entries)
{
	static unsigned char buff[IMAGE_SECTORS * SECTOR];
	struct archive_entry *ae;
	struct archive *a;
	int r;

	build_image(buff, logical_block_size);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_iso9660(a));
	r = archive_read_open_memory(a, buff, sizeof(buff));
	if (r == ARCHIVE_OK && read_entries) {
		/* Read the root directory entry and then EOF. */
		r = archive_read_next_header(a, &ae);
		if (r == ARCHIVE_OK)
			r = archive_read_next_header(a, &ae);
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	return (r);
}

DEFINE_TEST(test_read_format_iso_bad_block_size)
{
	/* Standard logical blocks are accepted and fully readable. */
	assertEqualInt(ARCHIVE_EOF, read_image(2048, 1));
	assertEqualInt(ARCHIVE_EOF, read_image(1024, 1));
	assertEqualInt(ARCHIVE_EOF, read_image(512, 1));

	/*
	 * Non-standard logical block sizes must be rejected by the
	 * bidder, so the open itself fails.
	 */

	/* Out-of-range powers of two. */
	assertEqualInt(ARCHIVE_FATAL, read_image(4096, 0));
	assertEqualInt(ARCHIVE_FATAL, read_image(256, 0));
	assertEqualInt(ARCHIVE_FATAL, read_image(128, 0));

	/* Non-powers of two */
	assertEqualInt(ARCHIVE_FATAL, read_image(65535, 0));
	assertEqualInt(ARCHIVE_FATAL, read_image(2049, 0));
	assertEqualInt(ARCHIVE_FATAL, read_image(2047, 0));
	assertEqualInt(ARCHIVE_FATAL, read_image(1, 0));
	assertEqualInt(ARCHIVE_FATAL, read_image(0, 0));
}
