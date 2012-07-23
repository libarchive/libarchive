/*-
 * Copyright (c) 2012 Matthias Brantner
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

static unsigned long
bitcrc32(unsigned long c, void *_p, size_t s)
{
	/* This is a drop-in replacement for crc32() from zlib.
	 * Libarchive should be able to correctly generate
	 * uncompressed zip archives (including correct CRCs) even
	 * when zlib is unavailable, and this function helps us verify
	 * that.  Yes, this is very, very slow and unsuitable for
	 * production use, but it's correct, compact, and works well
	 * enough for this particular usage.  Libarchive internally
	 * uses a much more efficient implementation.  */
	const unsigned char *p = _p;
	int bitctr;

	if (p == NULL)
		return (0);

	for (; s > 0; --s) {
		c ^= *p++;
		for (bitctr = 8; bitctr > 0; --bitctr) {
			if (c & 1) c = (c >> 1);
			else	   c = (c >> 1) ^ 0xedb88320;
			c ^= 0x80000000;
		}
	}
	return (c);
}

/* Quick and dirty: Read 2-byte and 4-byte integers from Zip file. */
static int i2(const char *p) { return ((p[0] & 0xff) | ((p[1] & 0xff) << 8)); }
static int i4(const char *p) { return (i2(p) | (i2(p + 2) << 16)); }

DEFINE_TEST(test_write_zip_set_compression_store)
{
	/* Buffer data */
	struct archive *a;
	struct archive_entry *entry;
	char buff[100000];
	const char *buffend;
	/* p is the pointer to walk over the central directory,
	 * q walks over the local headers, the data and the data descriptors. */
	const char *p, *q;
	size_t used;

	/* File data */
	char file_name[] = "file";
	char file_data1[] = {'1', '2', '3', '4', '5'};
	char file_data2[] = {'6', '7', '8', '9', '0'};
	int file_perm = 00644;
	short file_uid = 10;
	short file_gid = 20;

	/* Folder data */
	char folder_name[] = "folder/";
	int folder_perm = 00755;
	short folder_uid = 30;
	short folder_gid = 40;

	/* Time data */
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	/* Misc variables */
	unsigned long crc;

	/* Create new ZIP archive in memory without padding. */
	assert((a = archive_write_new()) != NULL);
	assertA(0 == archive_write_set_format_zip(a));
	assertA(0 == archive_write_set_compression_none(a));
	assertA(0 == archive_write_set_bytes_per_block(a, 1));
	assertA(0 == archive_write_set_bytes_in_last_block(a, 1));
	assertA(0 == archive_write_open_memory(a, buff, sizeof(buff), &used));

	/* Write entries. */

	/* Regular file */
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, file_name);
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_size(entry, sizeof(file_data1) + sizeof(file_data2));
	archive_entry_set_uid(entry, file_uid);
	archive_entry_set_gid(entry, file_gid);
	archive_entry_set_mtime(entry, t, 0);
	archive_entry_set_atime(entry, t, 0);
	archive_entry_set_ctime(entry, t, 0);
	archive_write_zip_set_compression_store(a);
	assertEqualIntA(a, 0, archive_write_header(a, entry));
	assertEqualIntA(a, sizeof(file_data1), archive_write_data(a, file_data1, sizeof(file_data1)));
	assertEqualIntA(a, sizeof(file_data2), archive_write_data(a, file_data2, sizeof(file_data2)));
	archive_entry_free(entry);
	archive_write_finish_entry(a);

	/* Folder */
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, folder_name);
	archive_entry_set_mode(entry, S_IFDIR | folder_perm);
	archive_entry_set_size(entry, 0);
	archive_entry_set_uid(entry, folder_uid);
	archive_entry_set_gid(entry, folder_gid);
	archive_entry_set_mtime(entry, t, 0);
	archive_entry_set_atime(entry, t, 0);
	archive_entry_set_ctime(entry, t, 0);
	archive_write_zip_set_compression_store(a);
	assertEqualIntA(a, 0, archive_write_header(a, entry));
	archive_entry_free(entry);
	archive_write_finish_entry(a);

	/* Close the archive . */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Remember the end of the archive in memory. */
	buffend = buff + used;

	/* Verify "End of Central Directory" record. */
	/* Get address of end-of-central-directory record. */
	p = buffend - 22; /* Assumes there is no zip comment field. */
	failure("End-of-central-directory begins with PK\\005\\006 signature");
	assertEqualMem(p, "PK\005\006", 4);
	failure("This must be disk 0");
	assertEqualInt(i2(p + 4), 0);
	failure("Central dir must start on disk 0");
	assertEqualInt(i2(p + 6), 0);
	failure("All central dir entries are on this disk");
	assertEqualInt(i2(p + 8), i2(p + 10));
	failure("CD start (%d) + CD length (%d) should == archive size - 22",
	    i4(p + 12), i4(p + 16));
	assertEqualInt(i4(p + 12) + i4(p + 16), used - 22);
	failure("no zip comment");
	assertEqualInt(i2(p + 20), 0);

	/* Get address of first entry in central directory. */
	p = buff + i4(buffend - 6);
	failure("Central file record at offset %d should begin with"
	    " PK\\001\\002 signature",
	    i4(buffend - 10));

	/* Verify file entry in central directory. */
	assertEqualMem(p, "PK\001\002", 4); /* Signature */
	assertEqualInt(i2(p + 4), 3 * 256 + 20); /* Version made by */
	assertEqualInt(i2(p + 6), 20); /* Version needed to extract */
	assertEqualInt(i2(p + 8), 8); /* Flags */
	assertEqualInt(i2(p + 10), 0); /* Compression method */
	assertEqualInt(i2(p + 12), (tm->tm_hour * 2048) + (tm->tm_min * 32) + (tm->tm_sec / 2)); /* File time */
	assertEqualInt(i2(p + 14), ((tm->tm_year - 80) * 512) + ((tm->tm_mon + 1) * 32) + tm->tm_mday); /* File date */
	crc = bitcrc32(0, file_data1, sizeof(file_data1));
	crc = bitcrc32(crc, file_data2, sizeof(file_data2));
	assertEqualInt(i4(p + 16), crc); /* CRC-32 */
	assertEqualInt(i4(p + 20), sizeof(file_data1) + sizeof(file_data2)); /* Compressed size */
	assertEqualInt(i4(p + 24), sizeof(file_data1) + sizeof(file_data2)); /* Uncompressed size */
	assertEqualInt(i2(p + 28), strlen(file_name)); /* Pathname length */
	assertEqualInt(i2(p + 30), 13); /* Extra field length */
	assertEqualInt(i2(p + 32), 0); /* File comment length */
	assertEqualInt(i2(p + 34), 0); /* Disk number start */
	assertEqualInt(i2(p + 36), 0); /* Internal file attrs */
	assertEqualInt(i4(p + 38) >> 16 & 01777, file_perm); /* External file attrs */
	assertEqualInt(i4(p + 42), 0); /* Offset of local header */
	assertEqualMem(p + 46, file_name, strlen(file_name)); /* Pathname */
	p = p + 46 + strlen(file_name);
	assertEqualInt(i2(p), 0x5455); /* 'UT' extension header */
	assertEqualInt(i2(p + 2), 5); /* 'UT' size */
	assertEqualInt(p[4], 7); /* 'UT' flags */
	assertEqualInt(i4(p + 5), t); /* 'UT' mtime */
	p = p + 9;
	assertEqualInt(i2(p), 0x7875); /* 'ux' extension header */
	assertEqualInt(i2(p + 2), 0); /* 'ux' size */
	p = p + 4;

	/* Verify local header of file entry. */
	q = buff;
	assertEqualMem(q, "PK\003\004", 4); /* Signature */
	assertEqualInt(i2(q + 4), 20); /* Version needed to extract */
	assertEqualInt(i2(q + 6), 8); /* Flags */
	assertEqualInt(i2(q + 8), 0); /* Compression method */
	assertEqualInt(i2(q + 10), (tm->tm_hour * 2048) + (tm->tm_min * 32) + (tm->tm_sec / 2)); /* File time */
	assertEqualInt(i2(q + 12), ((tm->tm_year - 80) * 512) + ((tm->tm_mon + 1) * 32) + tm->tm_mday); /* File date */
	assertEqualInt(i4(q + 14), 0); /* CRC-32 */
	assertEqualInt(i4(q + 18), sizeof(file_data1) + sizeof(file_data2)); /* Compressed size */
	assertEqualInt(i4(q + 22), sizeof(file_data1) + sizeof(file_data2)); /* Uncompressed size */
	assertEqualInt(i2(q + 26), strlen(file_name)); /* Pathname length */
	assertEqualInt(i2(q + 28), 32); /* Extra field length */
	assertEqualMem(q + 30, file_name, strlen(file_name)); /* Pathname */
	q = q + 30 + strlen(file_name);
	assertEqualInt(i2(q), 0x5455); /* 'UT' extension header */
	assertEqualInt(i2(q + 2), 13); /* 'UT' size */
	assertEqualInt(q[4], 7); /* 'UT' flags */
	assertEqualInt(i4(q + 5), t); /* 'UT' mtime */
	assertEqualInt(i4(q + 9), t); /* 'UT' atime */
	assertEqualInt(i4(q + 13), t); /* 'UT' ctime */
	q = q + 17;
	assertEqualInt(i2(q), 0x7875); /* 'ux' extension header */
	assertEqualInt(i2(q + 2), 11); /* 'ux' size */
	assertEqualInt(q[4], 1); /* 'ux' version */
	assertEqualInt(q[5], 4); /* 'ux' uid size */
	assertEqualInt(i4(q + 6), file_uid); /* 'Ux' UID */
	assertEqualInt(q[10], 4); /* 'ux' gid size */
	assertEqualInt(i4(q + 11), file_gid); /* 'Ux' GID */
	q = q + 15;

	/* Verify data of file entry. */
	assertEqualMem(q, file_data1, sizeof(file_data1));
	assertEqualMem(q + sizeof(file_data1), file_data2, sizeof(file_data2));
	q = q + sizeof(file_data1) + sizeof(file_data2);

	/* Verify data descriptor of file entry. */
	assertEqualMem(q, "PK\007\010", 4); /* Signature */
	assertEqualInt(i4(q + 4), crc); /* CRC-32 */
	assertEqualInt(i4(q + 8), sizeof(file_data1) + sizeof(file_data2)); /* Compressed size */
	assertEqualInt(i4(q + 12), sizeof(file_data1) + sizeof(file_data2)); /* Uncompressed size */
	q = q + 16;

	/* Verify folder entry in central directory. */
	assertEqualMem(p, "PK\001\002", 4); /* Signature */
	assertEqualInt(i2(p + 4), 3 * 256 + 20); /* Version made by */
	assertEqualInt(i2(p + 6), 20); /* Version needed to extract */
	assertEqualInt(i2(p + 8), 8); /* Flags */
	assertEqualInt(i2(p + 10), 0); /* Compression method */
	assertEqualInt(i2(p + 12), (tm->tm_hour * 2048) + (tm->tm_min * 32) + (tm->tm_sec / 2)); /* File time */
	assertEqualInt(i2(p + 14), ((tm->tm_year - 80) * 512) + ((tm->tm_mon + 1) * 32) + tm->tm_mday); /* File date */
	crc = 0;
	assertEqualInt(i4(p + 16), crc); /* CRC-32 */
	assertEqualInt(i4(p + 20), 0); /* Compressed size */
	assertEqualInt(i4(p + 24), 0); /* Uncompressed size */
	assertEqualInt(i2(p + 28), strlen(folder_name)); /* Pathname length */
	assertEqualInt(i2(p + 30), 13); /* Extra field length */
	assertEqualInt(i2(p + 32), 0); /* File comment length */
	assertEqualInt(i2(p + 34), 0); /* Disk number start */
	assertEqualInt(i2(p + 36), 0); /* Internal file attrs */
	assertEqualInt(i4(p + 38) >> 16 & 01777, folder_perm); /* External file attrs */
	assertEqualInt(i4(p + 42), q - buff); /* Offset of local header */
	assertEqualMem(p + 46, folder_name, strlen(folder_name)); /* Pathname */
	p = p + 46 + strlen(folder_name);
	assertEqualInt(i2(p), 0x5455); /* 'UT' extension header */
	assertEqualInt(i2(p + 2), 5); /* 'UT' size */
	assertEqualInt(p[4], 7); /* 'UT' flags */
	assertEqualInt(i4(p + 5), t); /* 'UT' mtime */
	p = p + 9;
	assertEqualInt(i2(p), 0x7875); /* 'ux' extension header */
	assertEqualInt(i2(p + 2), 0); /* 'ux' size */
	/*p = p + 4;*/

	/* Verify local header of folder entry. */
	assertEqualMem(q, "PK\003\004", 4); /* Signature */
	assertEqualInt(i2(q + 4), 20); /* Version needed to extract */
	assertEqualInt(i2(q + 6), 8); /* Flags */
	assertEqualInt(i2(q + 8), 0); /* Compression method */
	assertEqualInt(i2(q + 10), (tm->tm_hour * 2048) + (tm->tm_min * 32) + (tm->tm_sec / 2)); /* File time */
	assertEqualInt(i2(q + 12), ((tm->tm_year - 80) * 512) + ((tm->tm_mon + 1) * 32) + tm->tm_mday); /* File date */
	assertEqualInt(i4(q + 14), 0); /* CRC-32 */
	assertEqualInt(i4(q + 18), 0); /* Compressed size */
	assertEqualInt(i4(q + 22), 0); /* Uncompressed size */
	assertEqualInt(i2(q + 26), strlen(folder_name)); /* Pathname length */
	assertEqualInt(i2(q + 28), 32); /* Extra field length */
	assertEqualMem(q + 30, folder_name, strlen(folder_name)); /* Pathname */
	q = q + 30 + strlen(folder_name);
	assertEqualInt(i2(q), 0x5455); /* 'UT' extension header */
	assertEqualInt(i2(q + 2), 13); /* 'UT' size */
	assertEqualInt(q[4], 7); /* 'UT' flags */
	assertEqualInt(i4(q + 5), t); /* 'UT' mtime */
	assertEqualInt(i4(q + 9), t); /* 'UT' atime */
	assertEqualInt(i4(q + 13), t); /* 'UT' ctime */
	q = q + 17;
	assertEqualInt(i2(q), 0x7875); /* 'ux' extension header */
	assertEqualInt(i2(q + 2), 11); /* 'ux' size */
	assertEqualInt(q[4], 1); /* 'ux' version */
	assertEqualInt(q[5], 4); /* 'ux' uid size */
	assertEqualInt(i4(q + 6), folder_uid); /* 'ux' UID */
	assertEqualInt(q[10], 4); /* 'ux' gid size */
	assertEqualInt(i4(q + 11), folder_gid); /* 'ux' GID */
	q = q + 15;

	/* There should not be any data in the folder entry,
	 * meaning next is the data descriptor header. */

	/* Verify data descriptor of folder entry. */
	assertEqualMem(q, "PK\007\010", 4); /* Signature */
	assertEqualInt(i4(q + 4), crc); /* CRC-32 */
	assertEqualInt(i4(q + 8), 0); /* Compressed size */
	assertEqualInt(i4(q + 12), 0); /* Uncompressed size */
	/*q = q + 16;*/
}
