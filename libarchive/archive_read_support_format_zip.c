/*-
 * Copyright (c) 2004 Tim Kientzle
 * Copyright (c) 2011 Michihiro NAKAJIMA
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

#include "archive_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read_support_format_zip.c 201102 2009-12-28 03:11:36Z kientzle $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <time.h>
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_entry_locale.h"
#include "archive_private.h"
#include "archive_read_private.h"
#include "archive_endian.h"

#ifndef HAVE_ZLIB_H
#include "archive_crc32.h"
#endif

struct zip_entry {
	int64_t			local_header_offset;
	int64_t			compressed_size;
	int64_t			uncompressed_size;
	int64_t			gid;
	int64_t			uid;
	struct archive_entry	*entry;
	time_t			mtime;
	time_t			atime;
	time_t			ctime;
	uint32_t		crc32;
	uint16_t		mode;
	uint16_t		flags;
	char			compression;
	char			system;
	char			have_central_directory;
};

struct zip {
	/* Structural information about the archive. */
	int64_t			central_directory_offset;
	size_t			central_directory_size;
	size_t			central_directory_entries;

	/* List of entries (seekable Zip only) */
	size_t			entries_remaining;
	struct zip_entry	*zip_entries;
	struct zip_entry	*entry;

	/* entry_bytes_remaining is the number of bytes we expect. */
	int64_t			entry_bytes_remaining;
	int64_t			entry_offset;
	size_t			entry_bytes_unconsumed;

	/* These count the number of bytes actually read for the entry. */
	int64_t			entry_compressed_bytes_read;
	int64_t			entry_uncompressed_bytes_read;

	/* Running CRC32 of the decompressed data */
	unsigned long		entry_crc32;

	/* Flags to mark progress of decompression. */
	char			decompress_init;
	char			end_of_entry;

	ssize_t			filename_length;
	ssize_t			extra_length;

	unsigned char 		*uncompressed_buffer;
	size_t 			uncompressed_buffer_size;
#ifdef HAVE_ZLIB_H
	z_stream		stream;
	char			stream_valid;
#endif

	struct archive_string	extra;
	struct archive_string_conv *sconv;
	struct archive_string_conv *sconv_default;
	struct archive_string_conv *sconv_utf8;
	int			init_default_conversion;
	char	format_name[64];
};

#define ZIP_LENGTH_AT_END	8
#define ZIP_UTF8_NAME		(1<<11)	

static int	archive_read_format_zip_streamable_bid(struct archive_read *, int);
static int	archive_read_format_zip_seekable_bid(struct archive_read *, int);
static int	archive_read_format_zip_options(struct archive_read *,
		    const char *, const char *);
static int	archive_read_format_zip_cleanup(struct archive_read *);
static int	archive_read_format_zip_read_data(struct archive_read *,
		    const void **, size_t *, int64_t *);
static int	archive_read_format_zip_read_data_skip(struct archive_read *a);
static int	archive_read_format_zip_seekable_read_header(struct archive_read *,
		    struct archive_entry *);
static int	archive_read_format_zip_streamable_read_header(struct archive_read *,
		    struct archive_entry *);
static int	search_next_signature(struct archive_read *);
static int	zip_read_data_deflate(struct archive_read *a, const void **buff,
		    size_t *size, int64_t *offset);
static int	zip_read_data_none(struct archive_read *a, const void **buff,
		    size_t *size, int64_t *offset);
static int	zip_read_local_file_header(struct archive_read *a,
    struct archive_entry *entry, struct zip *);
static time_t	zip_time(const char *);
static const char *compression_name(int compression);
static void process_extra(const char *, size_t, struct zip_entry *);

int
archive_read_support_format_zip_streamable(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct zip *zip;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_zip");

	zip = (struct zip *)malloc(sizeof(*zip));
	if (zip == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate zip data");
		return (ARCHIVE_FATAL);
	}
	memset(zip, 0, sizeof(*zip));

	r = __archive_read_register_format(a,
	    zip,
	    "zip",
	    archive_read_format_zip_streamable_bid,
	    archive_read_format_zip_options,
	    archive_read_format_zip_streamable_read_header,
	    archive_read_format_zip_read_data,
	    archive_read_format_zip_read_data_skip,
	    archive_read_format_zip_cleanup);

	if (r != ARCHIVE_OK)
		free(zip);
	return (ARCHIVE_OK);
}

int
archive_read_support_format_zip_seekable(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct zip *zip;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_zip_seekable");

	zip = (struct zip *)malloc(sizeof(*zip));
	if (zip == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate zip data");
		return (ARCHIVE_FATAL);
	}
	memset(zip, 0, sizeof(*zip));

	r = __archive_read_register_format(a,
	    zip,
	    "zip",
	    archive_read_format_zip_seekable_bid,
	    archive_read_format_zip_options,
	    archive_read_format_zip_seekable_read_header,
	    archive_read_format_zip_read_data,
	    archive_read_format_zip_read_data_skip,
	    archive_read_format_zip_cleanup);

	if (r != ARCHIVE_OK)
		free(zip);
	return (ARCHIVE_OK);
}

int
archive_read_support_format_zip(struct archive *a)
{
	int r;
	r = archive_read_support_format_zip_streamable(a);
	if (r != ARCHIVE_OK)
		return r;
	return (archive_read_support_format_zip_seekable(a));
}

/*
 * TODO: This is a performance sink because it forces
 * the read core to drop buffered data from the start
 * of file, which will then have to be re-read again
 * if this bidder loses.
 *
 * Consider passing in the winning bid value to subsequent
 * bidders so that this bidder in particular can avoid
 * seeking if it knows it's going to lose anyway.
 */
static int
archive_read_format_zip_seekable_bid(struct archive_read *a, int best_bid)
{
	struct zip *zip = (struct zip *)a->format->data;
	int64_t filesize;
	const char *p;

	/* If someone has already bid more than 32, then avoid
	   trashing the look-ahead buffers with a seek. */
	if (best_bid > 32)
		return (-1);

	filesize = __archive_read_seek(a, -22, SEEK_END);
	/* If we can't seek, then we can't bid. */
	if (filesize <= 0)
		return 0;

	/* TODO: More robust search for end of central directory record. */
	if ((p = __archive_read_ahead(a, 22, NULL)) == NULL)
		return 0;
	/* First four bytes are signature for end of central directory
	   record.  Four zero bytes ensure this isn't a multi-volume
	   Zip file (which we don't yet support). */
	if (memcmp(p, "PK\005\006\000\000\000\000", 8) != 0)
		return 0;

	/* Since we've already done the hard work of finding the
	   end of central directory record, let's save the important
	   information. */
	zip->central_directory_entries = archive_le16dec(p + 10);
	zip->central_directory_size = archive_le32dec(p + 12);
	zip->central_directory_offset = archive_le32dec(p + 16);

	/* Just one volume, so central dir must all be on this volume. */
	if (zip->central_directory_entries != archive_le16dec(p + 8))
		return 0;
	/* Central directory can't extend beyond end of this file. */
	if (zip->central_directory_offset + zip->central_directory_size > filesize)
		return 0;

	/* This is just a tiny bit higher than the maximum returned by
	   the streaming Zip bidder.  This ensures that the more accurate
	   seeking Zip parser wins whenever seek is available. */
	return 32;
}

static int
slurp_central_directory(struct archive_read *a, struct zip *zip)
{
	int i;

	__archive_read_seek(a, zip->central_directory_offset, SEEK_SET);

	zip->zip_entries = calloc(zip->central_directory_entries, sizeof(struct zip_entry));
	for (i = 0; i < zip->central_directory_entries; ++i) {
		struct zip_entry *zip_entry = &zip->zip_entries[i];
		size_t filename_length, extra_length, comment_length;
		uint32_t external_attributes;
		const char *p;

		if ((p = __archive_read_ahead(a, 46, NULL)) == NULL)
			return ARCHIVE_FATAL;
		if (memcmp(p, "PK\001\002", 4) != 0) {
			archive_set_error(&a->archive,
			    -1, "Invalid central directory signature");
			return ARCHIVE_FATAL;
		}
		zip_entry->have_central_directory = 1;
		/* version = p[4]; */
		zip_entry->system = p[5];
		/* version_required = archive_le16dec(p + 6); */
		zip_entry->flags = archive_le16dec(p + 8);
		zip_entry->compression = archive_le16dec(p + 10);
		zip_entry->mtime = zip_time(p + 12);
		zip_entry->crc32 = archive_le32dec(p + 16);
		zip_entry->compressed_size = archive_le32dec(p + 20);
		zip_entry->uncompressed_size = archive_le32dec(p + 24);
		filename_length = archive_le16dec(p + 28);
		extra_length = archive_le16dec(p + 30);
		comment_length = archive_le16dec(p + 32);
		/* disk_start = archive_le16dec(p + 34); */ /* Better be zero. */
		/* internal_attributes = archive_le16dec(p + 36); */ /* text bit */
		external_attributes = archive_le32dec(p + 38);
		zip_entry->local_header_offset = archive_le32dec(p + 42);

		if (zip_entry->system == 3) {
			zip_entry->mode = external_attributes >> 16;
		} else {
			zip_entry->mode = AE_IFREG | 0777;
		}

		/* Do we need to parse filename here? */
		/* Or can we wait until we read the local header? */
		__archive_read_consume(a,
		    46 + filename_length + extra_length + comment_length);
	}

	/* TODO: Sort zip entries. */

	return ARCHIVE_OK;
}

static int
archive_read_format_zip_seekable_read_header(struct archive_read *a,
	struct archive_entry *entry)
{
	struct zip *zip = (struct zip *)a->format->data;
	const char *h;
	int r;

	a->archive.archive_format = ARCHIVE_FORMAT_ZIP;
	if (a->archive.archive_format_name == NULL)
		a->archive.archive_format_name = "ZIP";

	if (zip->zip_entries == NULL) {
		r = slurp_central_directory(a, zip);
		zip->entries_remaining = zip->central_directory_entries;
		if (r != ARCHIVE_OK)
			return r;
		zip->entry = zip->zip_entries;
	} else {
		++zip->entry;
	}

	if (zip->entries_remaining <= 0)
		return ARCHIVE_EOF;
	--zip->entries_remaining;

	zip->decompress_init = 0;
	zip->end_of_entry = 0;
	zip->entry_uncompressed_bytes_read = 0;
	zip->entry_compressed_bytes_read = 0;
	zip->entry_crc32 = crc32(0, NULL, 0);
	/* TODO: If entries are sorted by offset within the file, we
	   should be able to skip here instead of seeking.  Skipping is
	   typically faster (easier for I/O layer to optimize). */
	__archive_read_seek(a, zip->entry->local_header_offset, SEEK_SET);
	if ((h = __archive_read_ahead(a, 4, NULL)) == NULL)
		return (ARCHIVE_FATAL);

	if (memcmp(h, "PK\003\004", 4) != 0) {
		archive_set_error(&a->archive, -1, "Damaged Zip archive");
		return ARCHIVE_FATAL;
	}

	r = zip_read_local_file_header(a, entry, zip);
	return (r);
}

static int
archive_read_format_zip_streamable_bid(struct archive_read *a, int best_bid)
{
	const char *p;

	(void)best_bid; /* UNUSED */

	if ((p = __archive_read_ahead(a, 4, NULL)) == NULL)
		return (-1);

	/*
	 * Bid of 30 here is: 16 bits for "PK",
	 * next 16-bit field has four options (-2 bits).
	 * 16 + 16-2 = 30.
	 */
	if (p[0] == 'P' && p[1] == 'K') {
		if ((p[2] == '\001' && p[3] == '\002')
		    || (p[2] == '\003' && p[3] == '\004')
		    || (p[2] == '\005' && p[3] == '\006')
		    || (p[2] == '\007' && p[3] == '\010')
		    || (p[2] == '0' && p[3] == '0'))
			return (30);
	}

	return (0);
}

static int
archive_read_format_zip_options(struct archive_read *a,
    const char *key, const char *val)
{
	struct zip *zip;
	int ret = ARCHIVE_FAILED;

	zip = (struct zip *)(a->format->data);
	if (strcmp(key, "compat-2x")  == 0) {
		/* Handle filnames as libarchive 2.x */
		zip->init_default_conversion = (val != NULL) ? 1 : 0;
		ret = ARCHIVE_OK;
	} else if (strcmp(key, "hdrcharset")  == 0) {
		if (val == NULL || val[0] == 0)
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "zip: hdrcharset option needs a character-set name");
		else {
			zip->sconv = archive_string_conversion_from_charset(
			    &a->archive, val, 0);
			if (zip->sconv != NULL) {
				if (strcmp(val, "UTF-8") == 0)
					zip->sconv_utf8 = zip->sconv;
				ret = ARCHIVE_OK;
			} else
				ret = ARCHIVE_FATAL;
		}
	} else
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "zip: unknown keyword ``%s''", key);

	return (ret);
}

static int
archive_read_format_zip_streamable_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	const void *h;
	const char *signature;
	struct zip *zip;
	int r = ARCHIVE_OK, r1;

	a->archive.archive_format = ARCHIVE_FORMAT_ZIP;
	if (a->archive.archive_format_name == NULL)
		a->archive.archive_format_name = "ZIP";

	zip = (struct zip *)(a->format->data);
	zip->decompress_init = 0;
	zip->end_of_entry = 0;
	zip->entry_uncompressed_bytes_read = 0;
	zip->entry_compressed_bytes_read = 0;
	zip->entry_crc32 = crc32(0, NULL, 0);
	if ((h = __archive_read_ahead(a, 4, NULL)) == NULL)
		return (ARCHIVE_FATAL);

	signature = (const char *)h;

	/* If we don't see a PK signature here, scan forward. */
	if (signature[0] != 'P' || signature[1] != 'K') {
		r = search_next_signature(a);
		if (r != ARCHIVE_OK) {
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "Unable to find next valid PK marker");
			return (ARCHIVE_FATAL);
		}
		if ((h = __archive_read_ahead(a, 4, NULL)) == NULL)
			return (ARCHIVE_FATAL);
		signature = (const char *)h;
	}

	/*
	 * "PK00" signature is used for "split" archives that
	 * only have a single segment.  This means we can just
	 * skip the PK00; the first real file header should follow.
	 */
	if (signature[2] == '0' && signature[3] == '0') {
		__archive_read_consume(a, 4);
		if ((h = __archive_read_ahead(a, 4, NULL)) == NULL)
			return (ARCHIVE_FATAL);
		signature = (const char *)h;
		if (signature[0] != 'P' || signature[1] != 'K') {
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "No valid marker found after PK00");
			return (ARCHIVE_FATAL);
		}
	}

	if (signature[2] == '\001' && signature[3] == '\002') {
		/* Beginning of central directory. */
		return (ARCHIVE_EOF);
	}

	if (signature[2] == '\003' && signature[3] == '\004') {
		/* Regular file entry. */
		if (zip->zip_entries == NULL) {
			zip->zip_entries = malloc(sizeof(struct zip_entry));
			if (zip->zip_entries == NULL) {
				archive_set_error(&a->archive, ENOMEM, "Out  of memory");
				return ARCHIVE_FATAL;
			}
		}
		zip->entry = zip->zip_entries;
		memset(zip->entry, 0, sizeof(struct zip_entry));
		r1 = zip_read_local_file_header(a, entry, zip);
		if (r1 != ARCHIVE_OK)
			return (r1);
		return (r);
	}

	if (signature[2] == '\005' && signature[3] == '\006') {
		/* End of central directory. */
		return (ARCHIVE_EOF);
	}

	archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
	    "Unable to make sense of PK marker (%d,%d)",
	    signature[2], signature[3]);
	return (ARCHIVE_FATAL);
}

static int
search_next_signature(struct archive_read *a)
{
	const void *h;
	const char *p, *q;
	size_t skip;
	ssize_t bytes;
	int64_t skipped = 0;

	for (;;) {
		h = __archive_read_ahead(a, 4, &bytes);
		if (h == NULL)
			return (ARCHIVE_FATAL);
		p = h;
		q = p + bytes;

		while (p + 4 <= q) {
			if (p[0] == 'P' && p[1] == 'K') {
				if ((p[2] == '\001' && p[3] == '\002')
				    || (p[2] == '\003' && p[3] == '\004')
				    || (p[2] == '\005' && p[3] == '\006')
				    || (p[2] == '\007' && p[3] == '\010')
				    || (p[2] == '0' && p[3] == '0')) {
					skip = p - (const char *)h;
					__archive_read_consume(a, skip);
					return (ARCHIVE_OK);
				}
			}
			++p;
		}
		skip = p - (const char *)h;
		__archive_read_consume(a, skip);
		skipped += skip;
	}
}

static int
zip_read_local_file_header(struct archive_read *a, struct archive_entry *entry,
    struct zip *zip)
{
	const char *p;
	const void *h;
	const wchar_t *wp;
	const char *cp;
	size_t len, filename_length, extra_length;
	struct archive_string_conv *sconv;
	struct zip_entry *zip_entry = zip->entry;
	uint32_t crc32;
	int64_t compressed_size, uncompressed_size;
	int ret = ARCHIVE_OK;
	char version;

	/* Setup default conversion. */
	if (zip->sconv == NULL && !zip->init_default_conversion) {
		zip->sconv_default =
		    archive_string_default_conversion_for_read(&(a->archive));
		zip->init_default_conversion = 1;
	}

	if ((p = __archive_read_ahead(a, 30, NULL)) == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file header");
		return (ARCHIVE_FATAL);
	}

	version = p[4];
	zip_entry->system = p[5];
	zip_entry->flags = archive_le16dec(p + 6);
	zip_entry->compression = archive_le16dec(p + 8);
	zip_entry->mtime = zip_time(p + 10);
	crc32 = archive_le32dec(p + 14);
	compressed_size = archive_le32dec(p + 18);
	uncompressed_size = archive_le32dec(p + 22);
	filename_length = archive_le16dec(p + 26);
	extra_length = archive_le16dec(p + 28);

	__archive_read_consume(a, 30);

	if (zip_entry->have_central_directory) {
		/* If we had a central dir entry, we must have size information
		   as well, so ignore this flag. */
		zip_entry->flags &= ~ZIP_LENGTH_AT_END;
		/* If we have values from both local file header and
		   central directory, warn about mismatches which
		   might indicate a damaged file.  But some writers
		   always put zero in local header; don't bother
		   warning about that. */
		if (crc32 != 0 && crc32 != zip_entry->crc32) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Inconsistent CRC32 values");
			ret = ARCHIVE_WARN;
		}
		if (compressed_size != 0
		    && compressed_size != zip_entry->compressed_size) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Inconsistent compressed size");
			ret = ARCHIVE_WARN;
		}
		if (uncompressed_size != 0
		    && uncompressed_size != zip_entry->uncompressed_size) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Inconsistent uncompressed size");
			ret = ARCHIVE_WARN;
		}
	} else {
		zip_entry->crc32 = crc32;
		zip_entry->compressed_size = compressed_size;
		zip_entry->uncompressed_size = uncompressed_size;
	}

	/* Read the filename. */
	if ((h = __archive_read_ahead(a, filename_length, NULL)) == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file header");
		return (ARCHIVE_FATAL);
	}
	if (zip_entry->flags & ZIP_UTF8_NAME) {
		/* The filename is stored to be UTF-8. */
		if (zip->sconv_utf8 == NULL) {
			zip->sconv_utf8 =
			    archive_string_conversion_from_charset(
				&a->archive, "UTF-8", 1);
			if (zip->sconv_utf8 == NULL)
				return (ARCHIVE_FATAL);
		}
		sconv = zip->sconv_utf8;
	} else if (zip->sconv != NULL)
		sconv = zip->sconv;
	else
		sconv = zip->sconv_default;

	if (archive_entry_copy_pathname_l(entry,
	    h, filename_length, sconv) != 0) {
		if (errno == ENOMEM) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate memory for Pathname");
			return (ARCHIVE_FATAL);
		}
		archive_set_error(&a->archive,
		    ARCHIVE_ERRNO_FILE_FORMAT,
		    "Pathname cannot be converted "
		    "from %s to current locale.",
		    archive_string_conversion_charset_name(sconv));
		ret = ARCHIVE_WARN;
	}
	__archive_read_consume(a, filename_length);

	if (zip_entry->mode == 0) {
		/* Especially in streaming mode, we can end up
		   here without having seen any mode information.
		   Guess from the filename. */
		wp = archive_entry_pathname_w(entry);
		if (wp != NULL) {
			len = wcslen(wp);
			if (len > 0 && wp[len - 1] == L'/')
				zip_entry->mode = AE_IFDIR | 0777;
			else
				zip_entry->mode = AE_IFREG | 0777;
		} else {
			cp = archive_entry_pathname(entry);
			len = (cp != NULL)?strlen(cp):0;
			if (len > 0 && cp[len - 1] == '/')
				zip_entry->mode = AE_IFDIR | 0777;
			else
				zip_entry->mode = AE_IFREG | 0777;
		}
	}

	/* Read the extra data. */
	if ((h = __archive_read_ahead(a, extra_length, NULL)) == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file header");
		return (ARCHIVE_FATAL);
	}
	process_extra(h, extra_length, zip_entry);
	__archive_read_consume(a, extra_length);

	/* Populate some additional entry fields: */
	archive_entry_set_mode(entry, zip_entry->mode);
	archive_entry_set_uid(entry, zip_entry->uid);
	archive_entry_set_gid(entry, zip_entry->gid);
	archive_entry_set_mtime(entry, zip_entry->mtime, 0);
	archive_entry_set_ctime(entry, zip_entry->ctime, 0);
	archive_entry_set_atime(entry, zip_entry->atime, 0);
	/* Set the size only if it's meaningful. */
	if (0 == (zip_entry->flags & ZIP_LENGTH_AT_END))
		archive_entry_set_size(entry, zip_entry->uncompressed_size);

	zip->entry_bytes_remaining = zip_entry->compressed_size;
	zip->entry_offset = 0;

	/* If there's no body, force read_data() to return EOF immediately. */
	if (0 == (zip_entry->flags & ZIP_LENGTH_AT_END)
	    && zip->entry_bytes_remaining < 1)
		zip->end_of_entry = 1;

	/* Set up a more descriptive format name. */
	sprintf(zip->format_name, "ZIP %d.%d (%s)",
	    version / 10, version % 10,
	    compression_name(zip->entry->compression));
	a->archive.archive_format_name = zip->format_name;

	return (ret);
}

static const char *
compression_name(int compression)
{
	static const char *compression_names[] = {
		"uncompressed",
		"shrinking",
		"reduced-1",
		"reduced-2",
		"reduced-3",
		"reduced-4",
		"imploded",
		"reserved",
		"deflation"
	};

	if (compression <
	    sizeof(compression_names)/sizeof(compression_names[0]))
		return compression_names[compression];
	else
		return "??";
}

/* Convert an MSDOS-style date/time into Unix-style time. */
static time_t
zip_time(const char *p)
{
	int msTime, msDate;
	struct tm ts;

	msTime = (0xff & (unsigned)p[0]) + 256 * (0xff & (unsigned)p[1]);
	msDate = (0xff & (unsigned)p[2]) + 256 * (0xff & (unsigned)p[3]);

	memset(&ts, 0, sizeof(ts));
	ts.tm_year = ((msDate >> 9) & 0x7f) + 80; /* Years since 1900. */
	ts.tm_mon = ((msDate >> 5) & 0x0f) - 1; /* Month number. */
	ts.tm_mday = msDate & 0x1f; /* Day of month. */
	ts.tm_hour = (msTime >> 11) & 0x1f;
	ts.tm_min = (msTime >> 5) & 0x3f;
	ts.tm_sec = (msTime << 1) & 0x3e;
	ts.tm_isdst = -1;
	return mktime(&ts);
}

static int
archive_read_format_zip_read_data(struct archive_read *a,
    const void **buff, size_t *size, int64_t *offset)
{
	int r, reset_buff = 0;
	struct zip *zip;

	zip = (struct zip *)(a->format->data);

	if (zip->entry_bytes_unconsumed) {
		__archive_read_consume(a, zip->entry_bytes_unconsumed);
		zip->entry_bytes_unconsumed = 0;
	}

	/*
	 * If we hit end-of-entry last time, clean up and return
	 * ARCHIVE_EOF this time.
	 */
	if (zip->end_of_entry) {
		*offset = zip->entry_uncompressed_bytes_read;
		*size = 0;
		*buff = NULL;
		return (ARCHIVE_EOF);
	}

	switch(zip->entry->compression) {
	case 0:  /* No compression. */
		r =  zip_read_data_none(a, buff, size, offset);
		reset_buff = 1;
		break;
	case 8: /* Deflate compression. */
		r =  zip_read_data_deflate(a, buff, size, offset);
		if (zip->entry_bytes_unconsumed) {
			__archive_read_consume(a, zip->entry_bytes_unconsumed);
			zip->entry_bytes_unconsumed = 0;
		}
		break;
	default: /* Unsupported compression. */
		*buff = NULL;
		*size = 0;
		*offset = 0;
		/* Return a warning. */
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Unsupported ZIP compression method (%s)",
		    compression_name(zip->entry->compression));
		if (zip->entry->flags & ZIP_LENGTH_AT_END) {
			/*
			 * ZIP_LENGTH_AT_END requires us to
			 * decompress the entry in order to
			 * skip it, but we don't know this
			 * compression method, so we give up.
			 */
			r = ARCHIVE_FATAL;
		} else {
			/* We can't decompress this entry, but we will
			 * be able to skip() it and try the next entry. */
			r = ARCHIVE_WARN;
		}
		break;
	}
	if (r != ARCHIVE_OK)
		return (r);
	/* Update checksum */
	if (*size)
		zip->entry_crc32 = crc32(zip->entry_crc32, *buff, *size);
	/* If we hit the end, swallow any end-of-data marker. */
	if (zip->end_of_entry) {
		if (zip->entry->flags & ZIP_LENGTH_AT_END) {
			const char *p;

			/* since we're screwing with the exposed window,
			 * it's possible this will cross a forced move/realloc
			 * w/in that layer... in other words, the previous
			 * read_ahead's returned pointer isn't trustable.
			 * thus we redo the window, and reset buff if needed.
			 * note that decompression sidesteps this via the
			 * flushing of entry_bytes_unconsumed
			 */
			if (NULL == (p = __archive_read_ahead(a,
			    zip->entry_bytes_unconsumed + 16, NULL))) {
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Truncated ZIP end-of-file record");
				return (ARCHIVE_FATAL);
			}
			/* update the ptr, and set p to just the 16 bytes
			 * we care about */
			if (reset_buff) {
				*buff = p;
				p += zip->entry_bytes_unconsumed;
			}
			zip->entry_bytes_unconsumed += 16;
			zip->entry->crc32 = archive_le32dec(p + 4);
			zip->entry->compressed_size = archive_le32dec(p + 8);
			zip->entry->uncompressed_size = archive_le32dec(p + 12);
		}
		/* Check file size, CRC against these values. */
		if (zip->entry->compressed_size != zip->entry_compressed_bytes_read) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "ZIP compressed data is wrong size");
			return (ARCHIVE_WARN);
		}
		/* Size field only stores the lower 32 bits of the actual
		 * size. */
		if ((zip->entry->uncompressed_size & UINT32_MAX)
		    != (zip->entry_uncompressed_bytes_read & UINT32_MAX)) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "ZIP uncompressed data is wrong size");
			return (ARCHIVE_WARN);
		}
		/* Check computed CRC against header */
		if (zip->entry->crc32 != zip->entry_crc32) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "ZIP bad CRC: 0x%lx should be 0x%lx",
			    (unsigned long)zip->entry_crc32,
			    (unsigned long)zip->entry->crc32);
			return (ARCHIVE_WARN);
		}
	}

	/* Return EOF immediately if this is a non-regular file. */
	if (AE_IFREG != (zip->entry->mode & AE_IFMT))
		return (ARCHIVE_EOF);
	return (ARCHIVE_OK);
}

/*
 * Read "uncompressed" data.  According to the current specification,
 * if ZIP_LENGTH_AT_END is specified, then the size fields in the
 * initial file header are supposed to be set to zero.  This would, of
 * course, make it impossible for us to read the archive, since we
 * couldn't determine the end of the file data.  Info-ZIP seems to
 * include the real size fields both before and after the data in this
 * case (the CRC only appears afterwards), so this works as you would
 * expect.
 *
 * Returns ARCHIVE_OK if successful, ARCHIVE_FATAL otherwise, sets
 * zip->end_of_entry if it consumes all of the data.
 */
static int
zip_read_data_none(struct archive_read *a, const void **buff,
    size_t *size, int64_t *offset)
{
	struct zip *zip;
	ssize_t bytes_avail;

	zip = (struct zip *)(a->format->data);

	if (zip->entry_bytes_remaining == 0) {
		*buff = NULL;
		*size = 0;
		*offset = zip->entry_offset;
		zip->end_of_entry = 1;
		return (ARCHIVE_OK);
	}
	/*
	 * Note: '1' here is a performance optimization.
	 * Recall that the decompression layer returns a count of
	 * available bytes; asking for more than that forces the
	 * decompressor to combine reads by copying data.
	 */
	*buff = __archive_read_ahead(a, 1, &bytes_avail);
	if (bytes_avail <= 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file data");
		return (ARCHIVE_FATAL);
	}
	if (bytes_avail > zip->entry_bytes_remaining)
		bytes_avail = zip->entry_bytes_remaining;

	*size = bytes_avail;
	*offset = zip->entry_offset;
	zip->entry_offset += bytes_avail;
	zip->entry_bytes_remaining -= bytes_avail;
	zip->entry_uncompressed_bytes_read += bytes_avail;
	zip->entry_compressed_bytes_read += bytes_avail;
	zip->entry_bytes_unconsumed = bytes_avail;
	return (ARCHIVE_OK);
}

#ifdef HAVE_ZLIB_H
static int
zip_read_data_deflate(struct archive_read *a, const void **buff,
    size_t *size, int64_t *offset)
{
	struct zip *zip;
	ssize_t bytes_avail;
	const void *compressed_buff;
	int r;

	zip = (struct zip *)(a->format->data);

	/* If the buffer hasn't been allocated, allocate it now. */
	if (zip->uncompressed_buffer == NULL) {
		zip->uncompressed_buffer_size = 32 * 1024;
		zip->uncompressed_buffer
		    = (unsigned char *)malloc(zip->uncompressed_buffer_size);
		if (zip->uncompressed_buffer == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "No memory for ZIP decompression");
			return (ARCHIVE_FATAL);
		}
	}

	/* If we haven't yet read any data, initialize the decompressor. */
	if (!zip->decompress_init) {
		if (zip->stream_valid)
			r = inflateReset(&zip->stream);
		else
			r = inflateInit2(&zip->stream,
			    -15 /* Don't check for zlib header */);
		if (r != Z_OK) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Can't initialize ZIP decompression.");
			return (ARCHIVE_FATAL);
		}
		/* Stream structure has been set up. */
		zip->stream_valid = 1;
		/* We've initialized decompression for this stream. */
		zip->decompress_init = 1;
	}

	/*
	 * Note: '1' here is a performance optimization.
	 * Recall that the decompression layer returns a count of
	 * available bytes; asking for more than that forces the
	 * decompressor to combine reads by copying data.
	 */
	compressed_buff = __archive_read_ahead(a, 1, &bytes_avail);
	if (bytes_avail <= 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file body");
		return (ARCHIVE_FATAL);
	}

	/*
	 * A bug in zlib.h: stream.next_in should be marked 'const'
	 * but isn't (the library never alters data through the
	 * next_in pointer, only reads it).  The result: this ugly
	 * cast to remove 'const'.
	 */
	zip->stream.next_in = (Bytef *)(uintptr_t)(const void *)compressed_buff;
	zip->stream.avail_in = bytes_avail;
	zip->stream.total_in = 0;
	zip->stream.next_out = zip->uncompressed_buffer;
	zip->stream.avail_out = zip->uncompressed_buffer_size;
	zip->stream.total_out = 0;

	r = inflate(&zip->stream, 0);
	switch (r) {
	case Z_OK:
		break;
	case Z_STREAM_END:
		zip->end_of_entry = 1;
		break;
	case Z_MEM_ERROR:
		archive_set_error(&a->archive, ENOMEM,
		    "Out of memory for ZIP decompression");
		return (ARCHIVE_FATAL);
	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "ZIP decompression failed (%d)", r);
		return (ARCHIVE_FATAL);
	}

	/* Consume as much as the compressor actually used. */
	bytes_avail = zip->stream.total_in;
	zip->entry_bytes_unconsumed = bytes_avail;
	zip->entry_bytes_remaining -= bytes_avail;
	zip->entry_compressed_bytes_read += bytes_avail;

	*offset = zip->entry_offset;
	*size = zip->stream.total_out;
	zip->entry_uncompressed_bytes_read += *size;
	*buff = zip->uncompressed_buffer;
	zip->entry_offset += *size;
	return (ARCHIVE_OK);
}
#else
static int
zip_read_data_deflate(struct archive_read *a, const void **buff,
    size_t *size, int64_t *offset)
{
	*buff = NULL;
	*size = 0;
	*offset = 0;
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "libarchive compiled without deflate support (no libz)");
	return (ARCHIVE_FATAL);
}
#endif

static int
archive_read_format_zip_read_data_skip(struct archive_read *a)
{
	struct zip *zip;
	int64_t bytes_skipped;

	zip = (struct zip *)(a->format->data);

	#define flush_unconsumed()	\
	if (zip->entry_bytes_unconsumed) {							\
		__archive_read_consume(a, zip->entry_bytes_unconsumed);	\
		zip->entry_bytes_unconsumed = 0;						\
	}

	flush_unconsumed();

	/* If we've already read to end of data, we're done. */
	if (zip->end_of_entry)
		return (ARCHIVE_OK);

	/*
	 * If the length is at the end, we have no choice but
	 * to decompress all the data to find the end marker.
	 */
	if (zip->entry->flags & ZIP_LENGTH_AT_END) {
		size_t size;
		int64_t offset;
		int r;
		do {
			const void *buff = NULL;
			r = archive_read_format_zip_read_data(a, &buff,
			    &size, &offset);
			flush_unconsumed();
		} while (r == ARCHIVE_OK);
		return (r);
	}

	#undef flush_consumed

	/*
	 * If the length is at the beginning, we can skip the
	 * compressed data much more quickly.
	 */
	bytes_skipped = __archive_read_consume(a, zip->entry_bytes_remaining);
	if (bytes_skipped < 0)
		return (ARCHIVE_FATAL);

	/* This entry is finished and done. */
	zip->end_of_entry = 1;
	return (ARCHIVE_OK);
}

static int
archive_read_format_zip_cleanup(struct archive_read *a)
{
	struct zip *zip;

	zip = (struct zip *)(a->format->data);
#ifdef HAVE_ZLIB_H
	if (zip->stream_valid)
		inflateEnd(&zip->stream);
#endif
	free(zip->zip_entries);
	free(zip->uncompressed_buffer);
	archive_string_free(&(zip->extra));
	free(zip);
	(a->format->data) = NULL;
	return (ARCHIVE_OK);
}

/*
 * The extra data is stored as a list of
 *	id1+size1+data1 + id2+size2+data2 ...
 *  triplets.  id and size are 2 bytes each.
 */
static void
process_extra(const char *p, size_t extra_length, struct zip_entry* zip_entry)
{
	int offset = 0;
	while (offset < extra_length - 4)
	{
		unsigned short headerid = archive_le16dec(p + offset);
		unsigned short datasize = archive_le16dec(p + offset + 2);
		offset += 4;
		if (offset + datasize > extra_length)
			break;
#ifdef DEBUG
		fprintf(stderr, "Header id 0x%04x, length %d\n",
		    headerid, datasize);
#endif
		switch (headerid) {
		case 0x0001:
			/* Zip64 extended information extra field. */
			if (datasize >= 8)
				zip_entry->uncompressed_size =
				    archive_le64dec(p + offset);
			if (datasize >= 16)
				zip_entry->compressed_size =
				    archive_le64dec(p + offset + 8);
			break;
		case 0x5455:
		{
			/* Extended time field "UT". */
			int flags = p[offset];
			offset++;
			datasize--;
			/* Flag bits indicate which dates are present. */
			if (flags & 0x01)
			{
#ifdef DEBUG
				fprintf(stderr, "mtime: %lld -> %d\n",
				    (long long)zip_entry->mtime,
				    archive_le32dec(p + offset));
#endif
				if (datasize < 4)
					break;
				zip_entry->mtime = archive_le32dec(p + offset);
				offset += 4;
				datasize -= 4;
			}
			if (flags & 0x02)
			{
				if (datasize < 4)
					break;
				zip_entry->atime = archive_le32dec(p + offset);
				offset += 4;
				datasize -= 4;
			}
			if (flags & 0x04)
			{
				if (datasize < 4)
					break;
				zip_entry->ctime = archive_le32dec(p + offset);
				offset += 4;
				datasize -= 4;
			}
			break;
		}
		case 0x5855:
		{
			/* Info-ZIP Unix Extra Field (old version) "UX". */
			if (datasize >= 8) {
				zip_entry->atime = archive_le32dec(p + offset);
				zip_entry->mtime = archive_le32dec(p + offset + 4);
			}
			if (datasize >= 12) {
				zip_entry->uid = archive_le16dec(p + offset + 8);
				zip_entry->gid = archive_le16dec(p + offset + 10);
			}
			break;
		}
		case 0x7855:
			/* Info-ZIP Unix Extra Field (type 2) "Ux". */
#ifdef DEBUG
			fprintf(stderr, "uid %d gid %d\n",
			    archive_le16dec(p + offset),
			    archive_le16dec(p + offset + 2));
#endif
			if (datasize >= 2)
				zip_entry->uid = archive_le16dec(p + offset);
			if (datasize >= 4)
				zip_entry->gid = archive_le16dec(p + offset + 2);
			break;
		case 0x7875:
		{
			/* Info-Zip Unix Extra Field (type 3) "ux". */
			int uidsize = 0, gidsize = 0;

			if (datasize >= 1 && p[offset] == 1) {/* version=1 */
				if (datasize >= 4) {
					/* get a uid size. */
					uidsize = p[offset+1];
					if (uidsize == 2)
						zip_entry->uid = archive_le16dec(
						     p + offset + 2);
					else if (uidsize == 4 && datasize >= 6)
						zip_entry->uid = archive_le32dec(
						     p + offset + 2);
				}
				if (datasize >= (2 + uidsize + 3)) {
					/* get a gid size. */
					gidsize = p[offset+2+uidsize];
					if (gidsize == 2)
						zip_entry->gid = archive_le16dec(
						    p+offset+2+uidsize+1);
					else if (gidsize == 4 &&
					    datasize >= (2 + uidsize + 5))
						zip_entry->gid = archive_le32dec(
						    p+offset+2+uidsize+1);
				}
			}
			break;
		}
		default:
			break;
		}
		offset += datasize;
	}
#ifdef DEBUG
	if (offset != extra_length)
	{
		fprintf(stderr,
		    "Extra data field contents do not match reported size!\n");
	}
#endif
}
